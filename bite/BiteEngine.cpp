#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/futures/Future.h>
#include <folly/Unit.h>

#include <atomic>

#include "crypto/CryptographicValidationMode.h"
#include "crypto/DecryptedAESKeyList.h"
#include "crypto/AESKeyDecryptionShareList.h"
#include "node/ConsensusTypes.h"
#include "bite/BiteEngine.h"
#include "bite/BiteCodec.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"
#include "SkaleCommon.h"
#include "Log.h"
#include "crypto/ConsensusAESKeyDecryptionShare.h"
#include "crypto/MockupAESKeyDecryptionShare.h"
#include "crypto/ConsensusAESKeyDecryptionShareSet.h"
#include "crypto/MockupAESKeyDecryptionShareSet.h"
#include "crypto/AESKeyDecryptionShareList.h"
#include "libBLS/threshold_encryption/TEPublicKeyShare.h"
#include "libBLS/threshold_encryption/ThresholdEncryption.h"


ptr<BiteCiphertext> BiteEngine::tryGetEncryptedRegularTxFields(
            const ptr<Transaction> &_transaction, epoch_id _currentEpochId) {
    CHECK_STATE(_transaction);

    auto encryptedRegularTxData = _transaction->getRegularTxEncryptedData();

    if (encryptedRegularTxData) {
        return encryptedRegularTxData;
    }
    // if not set bite data already - try parse it
    auto ethTx = _transaction->getAsEthereumTransaction();
    
    ptr<BiteCiphertext> biteCiphertext;
    if (ethTx->hasToField()) {
        auto to = ethTx->getToField();
        auto data = ethTx->getTransactionDataField();
        CHECK_STATE(to);
        CHECK_STATE(data);

       biteCiphertext = BiteCodec::tryParseEncryptedRegularTxFields(*to, data, _currentEpochId);
        _transaction->setRegularTxEncryptedData(biteCiphertext); // cache it
    }

    return biteCiphertext;
}

ptr<std::vector<ptr<BiteCiphertext>>> BiteEngine::tryGetEncryptedCTXArgs(
            const ptr<Transaction>& _transaction, epoch_id _currentEpochId ) {
    CHECK_STATE(_transaction);

    auto encryptedCTXArgs = _transaction->getCTXEncryptedArgs();
    auto scAddressAadTE = _transaction->getScAddressAadTE();

    // if not cached - try to parse it
    if (!encryptedCTXArgs || !scAddressAadTE) {

        // if not set bite data already - try parse it
        auto ethTx = _transaction->getAsEthereumTransaction();
        
        auto dataField = ethTx->getTransactionDataField();
        CHECK_STATE(dataField);

        encryptedCTXArgs = BiteCodec::tryParseEncryptedCTXArgs(*dataField, _currentEpochId);
        
        if (encryptedCTXArgs) {
            // fetch & cache 'to' field - scAddress used as AAD for TE validation phases
            auto toField = ethTx->getToField();
            CHECK_STATE(toField);
            CHECK_STATE2(toField->size() == sizeof(AddressBytes), 
                "CTX transaction 'to' field must be exactly 20 bytes");
            
            AddressBytes toFieldBytes;
            std::copy(toField->begin(), toField->end(), toFieldBytes.begin());

            // only cache after both are successfully parsed
            _transaction->setScAddressAadTE(toFieldBytes);
            _transaction->setCTXEncryptedArgs(encryptedCTXArgs);
            
            // Fetch the cached values to return
            encryptedCTXArgs = _transaction->getCTXEncryptedArgs();
            scAddressAadTE = _transaction->getScAddressAadTE();
        }
    }
    return encryptedCTXArgs;
}

BiteEngine::ParseResult BiteEngine::parseAndCacheBITETransactions(
    const TransactionList& txList,
    BiteRuntimeContext& runtimeContext
) {
    ParseResult result;

    ptr<vector<ptr<Transaction> > > transactions = txList.getItems();
    // Assume no regular txs by default; only process regular txs if a non-CTX is found
    size_t regularTxsStartIdx = transactions->size();

    std::set<size_t> failedTxIndices;

    if ( runtimeContext.isBite2PatchEnabled ) {
        // Try parsing CTX transactions first
        for (size_t i = 0 ; i < transactions->size(); i++) {
            try {
                auto tx = transactions->at(i);
                auto ctxArgs = BiteEngine::tryGetEncryptedCTXArgs(tx, runtimeContext.currentEpoch);

                if (ctxArgs) {
                    // Fetch the cached SC address for AAD
                    auto scAddressAadTE = tx->getScAddressAadTE();
                    std::optional<AddressBytes> scAddrOpt = scAddressAadTE ? std::make_optional(*scAddressAadTE) : std::nullopt;

                    auto txCiphertexts = make_shared<TransactionCiphertexts>(*ctxArgs, scAddrOpt);
                    result.txsCiphertexts.emplace(i, txCiphertexts);
                } else {
                    // the first non-CTX transaction indicates the start of regular transactions
                    regularTxsStartIdx = i;
                    break;
                }
            } catch (exception &e) {
                CONS_LOG(err, fmt::format("Could not try to parse as CTX transaction: {}: ", i) + e.what());
                failedTxIndices.insert(i);
            }
        }
    } else {
        // No BITE2 means all txs are regular
        regularTxsStartIdx = 0;
    }

    // Parse regular txs
    for (size_t i = regularTxsStartIdx; i < transactions->size(); i++) {
        try {
            if (failedTxIndices.find(i) != failedTxIndices.end()) {
                // already reported failure for this tx (e.g., CTX parsing failed)
                // no need to try parsing again
                continue;
            }
            auto tx = transactions->at(i);
            auto ciphertext = BiteEngine::tryGetEncryptedRegularTxFields(tx, runtimeContext.currentEpoch);
            if (ciphertext) {
                auto txCiphertexts = make_shared<TransactionCiphertexts>(ciphertext);
                result.txsCiphertexts.emplace(i, txCiphertexts);
            }
        } catch (exception &e) {
            CONS_LOG(err, fmt::format("Could not try to parse as regular encrypted transaction: {}: ", i) + e.what());
            failedTxIndices.insert(i);
        }
    }

    // Populate failed transaction indices
    for (const auto& idx : failedTxIndices) {
        result.failedTransactions.push_back(idx);
    }

    return result;
}

// ================ Validate Ciphertexts =================== //

// ------ Helpers ------ //

namespace {

struct PendingTxBatch {
    transaction_index txIdx;
    ptr<TransactionCiphertexts> ciphertexts;
    size_t futureStartIdx = 0;
    size_t ciphertextCount = 0;
};

struct ScheduledCiphertextParsing {
    std::vector<PendingTxBatch> pendingTxs;
    std::vector<folly::Future<libBLS::CipheredKey>> parseFutures;
};

// Schedules individual CipheredKey::fromBytes() calls
// for all ciphertexts in the map, using the provided executor.
// 1 CipheredKey per folly future.
// @note Tested with batched version, setting batched deserializations
// per folly future, but the code complexity increased & the measured 
// performance benefit is not worth it.
ScheduledCiphertextParsing scheduleCiphertextParsing(
    const TransactionCiphertextsMap& txsCiphertexts,
    folly::Executor* executor,
    BiteEngine::CiphertextValidationResult& engineResult)
{
    ScheduledCiphertextParsing scheduled;
    scheduled.pendingTxs.reserve(txsCiphertexts.size());
    scheduled.parseFutures.reserve(txsCiphertexts.totalCiphertextCount());

    for (auto&& [idx, ciphertexts] : txsCiphertexts) {
        try {
            CHECK_STATE(ciphertexts);

            const size_t count = ciphertexts->count();
            const size_t futureStartIdx = scheduled.parseFutures.size();

            scheduled.pendingTxs.push_back(
                PendingTxBatch{idx, ciphertexts, futureStartIdx, count});

            for (size_t i = 0; i < count; ++i) {
                scheduled.parseFutures.push_back(
                    folly::via(executor, [ciphertexts, i]() {
                        return libBLS::CipheredKey::fromBytes((*ciphertexts)[i].data());
                    }));
            }
        }
        catch (exception& _e) {
            engineResult.invalidCiphertextIndices.push_back(idx);
            engineResult.failureReasons.push_back(
                "Ciphertext scheduling failed to start: " + std::string(_e.what()));
        }
    }

    return scheduled;
}

// Checks if any of the parse results for the given pending batch has an exception.
bool findParseFailure(
    const PendingTxBatch& pending,
    const std::vector<folly::Try<libBLS::CipheredKey>>& parseResults,
    size_t& failingIdx,
    std::string& parseError)
{
    for (size_t i = 0; i < pending.ciphertextCount; ++i) {
        const auto& result = parseResults[pending.futureStartIdx + i];
        if (!result.hasException()) {
            continue;
        }

        failingIdx = i;
        try {
            result.exception().throw_exception();
        }
        catch (const std::exception& e) {
            parseError = e.what();
        }
        catch (...) {
            parseError = "unknown exception";
        }
        return true;
    }

    return false;
}

void appendAADForTransaction(
    ptr<TransactionCiphertexts> ciphertexts,
    std::vector<std::vector<uint8_t>>& aadTE,
    bool& hasAnyAAD)
{
    if (!ciphertexts->isCTX()) {
        return;
    }

    const auto& scAddr = ciphertexts->getScAddressAadTE();
    CHECK_STATE(scAddr.has_value());

    std::vector<uint8_t> aadBytes(scAddr->begin(), scAddr->end());

    for (size_t i = 0; i < ciphertexts->count(); ++i) {
        aadTE.push_back(aadBytes);
    }

    hasAnyAAD = true;
}

void appendSemanticValidationFailures(
    const std::vector<std::pair<transaction_index, size_t>>& parsedTxs,
    const BiteCore::CiphertextValidationResult& coreResult,
    BiteEngine::CiphertextValidationResult& engineResult)
{
    size_t ciphertextGlobalIdx = 0;

    for (const auto& [idx, numCiphertexts] : parsedTxs) {
        bool alreadyHasOneInvalidCiphertext = false;

        for (size_t i = 0; i < numCiphertexts; ++i) {
            if (!coreResult.validationResults[ciphertextGlobalIdx] &&
                !alreadyHasOneInvalidCiphertext) {
                engineResult.invalidCiphertextIndices.push_back(idx);
                engineResult.failureReasons.push_back(
                    "Ciphertext with global index " +
                    std::to_string(ciphertextGlobalIdx) +
                    " failed semantic validation");
                alreadyHasOneInvalidCiphertext = true;
            }
            ciphertextGlobalIdx++;
        }
    }
}

} // anonymous namespace

// ------ Main function ------ //


BiteEngine::CiphertextValidationResult BiteEngine::validateCiphertexts(
    const TransactionCiphertextsMap& txsCiphertexts,
    const BiteRuntimeContext& runtimeContext) const
{
    CiphertextValidationResult engineResult;

    std::vector<libBLS::CipheredKey> cipheredKeys;
    cipheredKeys.reserve(txsCiphertexts.totalCiphertextCount());

    // AAD vector aligned with cipheredKeys - empty vector means no AAD for that ciphertext
    std::vector<std::vector<uint8_t>> aadTE;
    aadTE.reserve(txsCiphertexts.totalCiphertextCount());
    bool hasAnyAAD = false;

    // Track only transactions that successfully parsed, preserving order with cipheredKeys
    std::vector<std::pair<transaction_index, size_t>> parsedTxs;
    size_t expectedValidationResults = 0;

    // Phase 1: Schedule parsing all ciphertext from bytes & also validating individual ciphertexts
    auto scheduled = scheduleCiphertextParsing(
        txsCiphertexts,
        runtimeContext.threadPoolExecutor.get(),
        engineResult);

    auto parseResults = folly::collectAll(scheduled.parseFutures).get();

    // Build CipheredKey vector & identify invalid ones
    for (const auto& pending : scheduled.pendingTxs) {
        size_t failingIdx = 0;
        std::string parseError;

        // Look for exceptions during execution
        if (findParseFailure(pending, parseResults, failingIdx, parseError)) {
            engineResult.invalidCiphertextIndices.push_back(pending.txIdx);
            engineResult.failureReasons.push_back(
                "Ciphertext with index " + std::to_string(failingIdx) +
                " failed to parse: " + parseError);
            continue;
        }

        try {
            // add AAD for TE validation if it's a CTX.
            // updates the hashAnyAAD flag if at least one CTX is present in the batch
            appendAADForTransaction(pending.ciphertexts, aadTE, hasAnyAAD);
            
            for (size_t i = 0; i < pending.ciphertextCount; ++i) {
                cipheredKeys.push_back(
                    std::move(parseResults[pending.futureStartIdx + i]).value());
            }

            parsedTxs.emplace_back(pending.txIdx, pending.ciphertextCount);
            expectedValidationResults += pending.ciphertextCount;
        }
        catch (exception& _e) {
            engineResult.invalidCiphertextIndices.push_back(pending.txIdx);
            engineResult.failureReasons.push_back(
                "Ciphertext metadata validation failed: " + std::string(_e.what()));
        }
    }

    // From the successfully built CipheredKey vector, validate ciphertexts
    // Pass AAD only if we have at least one CTX transaction
    BiteCore::CiphertextValidationResult coreResult =
        core.validateCiphertexts(cipheredKeys, hasAnyAAD ? &aadTE : nullptr);

    CHECK_STATE(coreResult.validationResults.size() == expectedValidationResults);

    if (!coreResult.allValid) {
        appendSemanticValidationFailures(parsedTxs, coreResult, engineResult);
    }

    // Only add public decryption values if both
    // 1) ciphertext parsing was successful
    // 2) core validation marked all as valid
    if (engineResult.allValid() && coreResult.allValid) {
        engineResult.publicDecryptionValues =
            libBLS::CipheredKey::getDecryptionShareInputBatch(cipheredKeys);
    }

    return engineResult;
}


// ================ Merge AES Keys =================== //


std::shared_ptr<DecryptedAESKeyList> BiteEngine::mergeAESKeys(
    block_id _blockId,
    TransactionCiphertextsMap& _txCiphertexts,
    const std::map<schain_index, std::shared_ptr<AESKeyDecryptionShareList>>& _decryptionShareMap,
    const std::vector<libBLS::TEPublicKeyShare>& _tePublicKeyShares,
    const BiteRuntimeContext& _runtimeContext
) const {

    CHECK_STATE(!_decryptionShareMap.empty());
    CHECK_STATE(_decryptionShareMap.size() >= config.requiredSigners);

    auto firstDecryptionShareList = _decryptionShareMap.begin()->second;
    CHECK_STATE(firstDecryptionShareList);
    auto expectedSharesCount = firstDecryptionShareList->totalCiphertextSharesCount();
    for (auto&& [_, list] : _decryptionShareMap) {
        CHECK_STATE(list);
        CHECK_STATE(list->totalCiphertextSharesCount() == expectedSharesCount);
    }

    // Initialize decryption share sets & build CipheredKey vectors for TE validation

    // 1 per transaction (but each tx may contain multiple ciphertexts)
    std::map<transaction_index, ptr<AESKeyDecryptionShareSet>> decryptionShareSets;
    auto encryptions = std::make_shared<std::map<transaction_index, std::vector<libBLS::CipheredKey>>>();

    bool toValidate = false;
    for (auto&& [idx, shares] : firstDecryptionShareList->getDecryptionShares()) {
        decryptionShareSets[idx] =
            createAESDecryptionShareSetObject(_blockId, idx, shares->size());

        if (config.sgxEnabled) {
            auto& ciphertextsForCurrTx = *_txCiphertexts.at(idx);
            for (auto& ciphertext : ciphertextsForCurrTx) {
                // deserialize - no validation needed
                (*encryptions)[idx].push_back(
                    libBLS::CipheredKey::fromBytes(ciphertext.data(), toValidate));
            }
        }
    }

    std::vector<folly::Future<folly::Unit>> futures;
    futures.reserve(decryptionShareSets.size());

    auto aesKeys = make_shared< DecryptedAESKeyList >();
    auto aesKeysMutex = std::make_shared<std::mutex>();


    // Initialize decryption share sets & build CipheredKey vectors for TE validation

    auto processTx = [aesKeys, aesKeysMutex, &_txCiphertexts, 
        &_decryptionShareMap, &_tePublicKeyShares, encryptions, config = this->config
    ](transaction_index txId, ptr<AESKeyDecryptionShareSet> decryptionSet) -> folly::Unit {

        size_t numberOfCiphertexts = _txCiphertexts.at(txId)->count();
        // still not enough shares - validate & add more
        if (!decryptionSet->isEnough()) {

            // shares at libBLS level (ciphertext idx -> list of shares for that ciphertext)
            std::vector< std::vector< libBLS::TEDecryptionShare > > teShares;
            std::vector< std::vector< libBLS::TEPublicKeyShare > > publicKeys;
            // additional data to track decryptor indices
            std::vector< size_t > decryptorIndices;
            // shares at consensus level
            // each index holds a list of shares for all ciphertexts within current tx for some decryptor
            std::vector< ptr< AESKeyDecryptionShares > > sharesList;
            // initialize vectors
            teShares.assign(numberOfCiphertexts, {});
            publicKeys.assign(numberOfCiphertexts, {});
            sharesList.resize(config.totalSigners);
            for (size_t i = 0; i < config.totalSigners; ++i) {
                sharesList[i] = make_shared<AESKeyDecryptionShares>();
            }

            // collect all shares from all nodes for current Tx
            for ( auto&& [decryptorIdx, decryptionSharesList]: _decryptionShareMap) {
                try {
                    // shares for all ciphertexts within current tx from current decryptor
                    ptr<AESKeyDecryptionShares> ciphertextsShares = decryptionSharesList->getDecryptionShares(txId);
                    CHECK_STATE(ciphertextsShares);
                    CHECK_STATE(ciphertextsShares->size() == numberOfCiphertexts);

                    size_t decryptorIndex = (size_t)decryptorIdx - 1;
                    decryptorIndices.push_back(decryptorIndex);
                    sharesList.at(decryptorIndex) = ciphertextsShares;

                    if (config.sgxEnabled) {
                        for (size_t i = 0; i < numberOfCiphertexts; ++i) {
                            // this conversion only works when using real validation. Else, it will be of Mockup type
                            auto shareConsensus = std::dynamic_pointer_cast<ConsensusAESKeyDecryptionShare>(ciphertextsShares->at(i));
                            CHECK_STATE(shareConsensus);

                            auto shareTE = shareConsensus->getTEDecryptionShare();
                            teShares.at(i).push_back(*shareTE);
                            publicKeys.at(i).push_back(_tePublicKeyShares.at(decryptorIndex));
                        }
                    }

                }  catch ( const std::exception& ex ) {
                    CONS_LOG(err, std::string("Error during adding shares: ") + ex.what());
                }
            }

            if (config.sgxEnabled) {
                std::vector<bool> allSharesFromNodeAreValid;
                allSharesFromNodeAreValid.resize(config.totalSigners, true);

                // Prepare AAD for CTX transactions
                const auto& txCiphertexts = _txCiphertexts.at(txId);
                const std::vector<std::vector<uint8_t>>* aadPtr = nullptr;
                std::vector<std::vector<uint8_t>> aadVec;
                
                if (txCiphertexts->isCTX()) {
                    const auto& scAddr = txCiphertexts->getScAddressAadTE();
                    CHECK_STATE(scAddr.has_value());
                    // Single element - same AAD for the one ciphertext validated per call
                    aadVec.push_back(std::vector<uint8_t>(scAddr->begin(), scAddr->end()));
                    aadPtr = &aadVec;
                }

                for (size_t ciphertextId = 0; ciphertextId < numberOfCiphertexts; ++ciphertextId) {
                    std::vector<libBLS::CipheredKey> cipheredKeys{ encryptions->at(txId).at(ciphertextId) };

                    auto result = libBLS::ThresholdEncryption::validateDecryptionSharesBatch(
                            cipheredKeys, teShares.at(ciphertextId), publicKeys.at(ciphertextId),
                            aadPtr);

                    for (size_t shareId = 0; shareId < result.size(); ++shareId) {
                        if (!result[shareId]) {
                            // shares from this node are invalid
                            allSharesFromNodeAreValid[decryptorIndices[shareId]] = false;
                            CONS_LOG(err, fmt::format(
                                "Decryption share validation failed: tx_id={}, ciphertext_id={}, share_id={}",
                                (uint32_t)txId, ciphertextId, shareId)
                            );
                        }
                    }
                }

                // Only add shares for current transaction if all shares for all ciphertexts are valid for some node
                for (size_t i = 0; i < decryptorIndices.size(); ++i) {
                    if (allSharesFromNodeAreValid[decryptorIndices[i]]) {
                        decryptionSet->addDecryptionSharesFromSameDecryptor(sharesList[decryptorIndices[i]]);
                    }
                }
            }
            else {
                for (size_t i = 0; i < decryptorIndices.size(); ++i) {
                    // add al valid shares
                    decryptionSet->addDecryptionSharesFromSameDecryptor(sharesList[decryptorIndices[i]]);
                }
            }
        }

        if ( decryptionSet->isEnough() ) {
            auto keys = decryptionSet->verifyAndMergeAESKeys(_txCiphertexts.at(txId)->getCiphertexts());
            CHECK_STATE( keys );
            std::lock_guard<std::mutex> lock(*aesKeysMutex);
            aesKeys->addKeys( txId, *keys );
        }
        return folly::unit;
    };

    try {
        if (_runtimeContext.threadPoolExecutor) {
            for ( auto&& [txId, decryptionSet]: decryptionShareSets ) {
                auto txIdLocal = txId;
                auto decryptionSetLocal = decryptionSet;
                futures.emplace_back(folly::via(_runtimeContext.threadPoolExecutor.get(), [processTx, txIdLocal, decryptionSetLocal]() {
                    return processTx(txIdLocal, decryptionSetLocal);
                }));
            }
            folly::collectAll(futures).get();
        } else {
            for ( auto&& [txId, decryptionSet]: decryptionShareSets ) {
                processTx(txId, decryptionSet);
            }
        }
    } catch ( ... ) {
        folly::collectAll( futures ).get();
        throw;
    }

    CHECK_STATE2(aesKeys->totalDecryptedCiphertextsCount() == _txCiphertexts.totalCiphertextCount(),
        "Not all aes keys could be decrypted");
    return aesKeys;
}




DecryptedTransactions BiteEngine::decryptTransactionsListInParallel(
        const TransactionList &_transactionList,
        const DecryptedAESKeyList &_aesKeys,
        BiteRuntimeContext& runtimeContext
) const {
 
    auto decryptedFieldsMap = std::make_shared<DecryptedRegularTxsMap>();
    auto regularTxMapMutex = std::make_shared<std::mutex>();
    
    auto ctxTxsMap          = std::make_shared<DecryptedCTXTxsMap>();
    auto ctxTxsMapMutex     = std::make_shared<std::mutex>();

    auto txs = _transactionList.getItems();
    CHECK_STATE(txs);

    std::vector<folly::Future<folly::Unit>> futures;
    futures.reserve(_aesKeys.getSize());



    // Helper to avoid repeating folly::via boilerplate
    auto schedule = [&](auto &&fn) {
        futures.emplace_back(
            folly::via(runtimeContext.threadPoolExecutor.get(), std::forward<decltype(fn)>(fn))
        );
    };

    try {
        const std::size_t txCount = _transactionList.size();

        bool allCTXsParsed = false;

        for (std::size_t txIdx = 0; txIdx < txCount; ++txIdx) {
            auto tx = txs->at(txIdx);

            if ( runtimeContext.isBite2PatchEnabled ) {
                // ---------- Try CTX path first ----------
                if (!allCTXsParsed) {
                    ptr< std::vector<ptr<BiteCiphertext> > > encryptedArgs;
                    try {
                        encryptedArgs = tryGetEncryptedCTXArgs(tx, runtimeContext.currentEpoch);
                    } catch (const std::exception& e) {
                        CONS_LOG(warn, fmt::format("Could not try to parse as CTX encrypted transaction during decryption: {}: {}", txIdx, e.what()));
                    }
                    if (encryptedArgs) {
                        // CTX tx with no encrypted arguments — nothing to decrypt
                        if (encryptedArgs->empty()) {
                            std::lock_guard<std::mutex> lock(*ctxTxsMapMutex);
                            ctxTxsMap->emplace(txIdx, DecryptedCTXArgs{});
                            continue;
                        }
                        auto decryptedAESKey = _aesKeys.getKeys(txIdx);
                        CHECK_STATE(decryptedAESKey);
                        CHECK_STATE(encryptedArgs->size() == decryptedAESKey->size());

                        try {
                            schedule([corePtr = &this->core, encryptedArgs, decryptedAESKey, ctxTxsMap, txIdx, ctxTxsMapMutex]()
                            -> folly::Unit {
                                try {
                                    DecryptedCTXArgs decryptedData;
                                    decryptedData.args.reserve(encryptedArgs->size());

                                    for (std::size_t argIdx = 0; argIdx < encryptedArgs->size(); ++argIdx) {
                                        const auto argCiphertext = encryptedArgs->at(argIdx);
                                        CHECK_STATE(argCiphertext);
                                        decryptedData.args.push_back(
                                            BiteCodec::decryptCiphertext(*argCiphertext,
                                                decryptedAESKey->at(argIdx).getAesKey(),
                                                *corePtr
                                            )
                                        );
                                    }

                                    std::lock_guard<std::mutex> lock(*ctxTxsMapMutex);
                                    ctxTxsMap->emplace(txIdx, std::move(decryptedData));
                                } catch (const std::exception& e) {
                                    CONS_LOG(err, fmt::format("Corrupt CTX tx:{} that doesn't decrypt: {}", txIdx, e.what()));
                                    std::lock_guard<std::mutex> lock(*ctxTxsMapMutex);
                                    ctxTxsMap->emplace(txIdx, std::nullopt);
                                } catch (...) {
                                    CONS_LOG(err, fmt::format("Corrupt CTX tx:{} that doesn't decrypt: unknown exception", txIdx));
                                    std::lock_guard<std::mutex> lock(*ctxTxsMapMutex);
                                    ctxTxsMap->emplace(txIdx, std::nullopt);
                                }

                                return folly::unit;
                            });
                        } catch (const std::exception &e) {
                            CONS_LOG(err, fmt::format("Corrupt CTX tx:{} - couldn't schedule for decryption: {}", txIdx, e.what()));
                            std::lock_guard<std::mutex> lock(*ctxTxsMapMutex);
                            ctxTxsMap->emplace(txIdx, std::nullopt);
                        }

                        // This tx is CTX, do not treat it as regular
                        continue;
                    }
                    else {
                        // No more CTX transactions expected after the first non-CTX one
                        allCTXsParsed = true;
                    }
                }
            }

            // ---------- Regular BITE1-style encryption ----------
            ptr<BiteCiphertext> bite;
            try {
                bite = tryGetEncryptedRegularTxFields(tx, runtimeContext.currentEpoch);
            } catch (const std::exception& e) {
                CONS_LOG(warn, fmt::format("Could not try to parse as regular encrypted transaction during decryption: {}: {}", txIdx, e.what()));
            }

            if (bite) {
                auto decryptedAESKey = _aesKeys.getKeys(txIdx);
                CHECK_STATE(decryptedAESKey);
                CHECK_STATE(decryptedAESKey->size() == 1); // single AES key expected

                try {
                    schedule([corePtr = &this->core, bite, decryptedAESKey, decryptedFieldsMap, txIdx, regularTxMapMutex]()
                    -> folly::Unit {
                        try {
                            auto decryptedTransactionFields = BiteCodec::decryptCiphertext(*bite,
                                    decryptedAESKey->at(0).getAesKey(),
                                    *corePtr
                                );

                            auto parsedRegularTx =
                                BiteCodec::parseRegularTxDecryptedData(decryptedTransactionFields);

                            std::lock_guard<std::mutex> lock(*regularTxMapMutex);
                            decryptedFieldsMap->emplace(txIdx, std::move(parsedRegularTx));
                        } catch (const std::exception& e) {
                            CONS_LOG(err, fmt::format("Corrupt regular tx:{} that doesn't decrypt: {}", txIdx, e.what()));
                            std::lock_guard<std::mutex> lock(*regularTxMapMutex);
                            decryptedFieldsMap->emplace(txIdx, std::nullopt);
                        } catch (...) {
                            CONS_LOG(err, fmt::format("Corrupt regular tx:{} that doesn't decrypt: unknown exception", txIdx));
                            std::lock_guard<std::mutex> lock(*regularTxMapMutex);
                            decryptedFieldsMap->emplace(txIdx, std::nullopt);
                        }

                        return folly::unit;
                    });
                } catch (const std::exception &e) {
                    CONS_LOG(err, fmt::format("Corrupt regular tx:{} - couldn't schedule for decryption: {}", txIdx, e.what()));
                    std::lock_guard<std::mutex> lock(*regularTxMapMutex);
                    decryptedFieldsMap->emplace(txIdx, std::nullopt);
                }

                continue;
            }

            // ---------- No BITE data at all ----------
            // If it's neither CTX nor regular encrypted, we must not have AES keys
            CHECK_STATE(!_aesKeys.getKeys(txIdx));
        }
    } catch ( ... ) {
        folly::collectAll( futures ).get();
        try {
            throw;
        }
        CATCH_LOG_AND_RETHROW_ANY_EXCEPTION(err, "Could not parse BITE transaction");
    }

    folly::collectAll(futures).get();

    return DecryptedTransactions(
        ctxTxsMap,
        decryptedFieldsMap 
    );

}


std::vector<uint8_t> BiteEngine::buildRegularTxData(
    const libBLS::TEPublicKey& key,
    const std::vector<uint8_t>& plainData,
    const std::vector<uint8_t>& to,
    uint64_t epochId
) const {
    auto payload = BiteCodec::encodeRegularTxPayload(plainData, to);
    auto cipher = core.encryptData(key, payload);      // core uses doRealCrypto internally
    return BiteCodec::encodeEpochedBiteData(cipher, epochId);
}

std::vector<uint8_t> BiteEngine::buildCTXData(
    const libBLS::TEPublicKey& key,
    size_t numberOfCiphertexts,
    uint64_t epochId,
    const std::optional<AddressBytes>& scAddressAadTE
) const {
    std::vector<std::vector<uint8_t>> encryptedSerializedArgs;
    encryptedSerializedArgs.reserve(numberOfCiphertexts);

    // Convert AddressBytes to vector<uint8_t> for AAD if present
    std::optional<std::vector<uint8_t>> aadVec = std::nullopt;
    if (scAddressAadTE) {
        aadVec = std::vector<uint8_t>(scAddressAadTE->begin(), scAddressAadTE->end());
    }

    for (size_t i = 0; i < numberOfCiphertexts; ++i) {
        std::vector<uint8_t> rndData(numberOfCiphertexts * 10);
        auto encryptedData = core.encryptData(key, rndData, aadVec);

        encryptedSerializedArgs.push_back(
            BiteCodec::encodeEpochedBiteData(encryptedData, epochId)
        );
    }

    std::vector<std::vector<uint8_t>> plainArgs;
    const size_t numPlaintexts = numberOfCiphertexts - 1;
    plainArgs.reserve(numPlaintexts);

    for (size_t i = 0; i < numPlaintexts; ++i) {
        plainArgs.emplace_back(numberOfCiphertexts * 5);
    }

    return BiteCodec::encodeCTXData(encryptedSerializedArgs, plainArgs);
}


std::shared_ptr<AESKeyDecryptionShares> BiteEngine::createDecryptionSharesObjects(
    const std::vector<std::string_view>& shareStrs,
    schain_index decryptorIndex,
    bool decryptionFailed, 
    CryptographicValidationMode validationMode
) const {
    auto shares = std::make_shared<AESKeyDecryptionShares>();
    for (auto shareStr : shareStrs) {
        std::string s(shareStr);
        if (usingRealCrypto()) {
            shares->push_back(
                std::make_shared<ConsensusAESKeyDecryptionShare>(
                    s, decryptorIndex, decryptionFailed, validationMode));
        } else {
            shares->push_back(
                std::make_shared<MockupAESKeyDecryptionShare>(
                    s, decryptorIndex, decryptionFailed));
        }
    }
    return shares;
}

ptr<AESKeyDecryptionShareSet> BiteEngine::createAESDecryptionShareSetObject(
        block_id _blockId, transaction_index _transactionIndex, size_t numberOfCiphertexts) const {
    if (usingRealCrypto()) {
        return make_shared<ConsensusAESKeyDecryptionShareSet>(
                _blockId, _transactionIndex, numberOfCiphertexts, config.totalSigners, config.requiredSigners);
    } else {
        return make_shared<MockupAESKeyDecryptionShareSet>(
                _blockId, _transactionIndex, config.totalSigners, config.requiredSigners);
    }
}

ptr<vector<ptr<AESKeyDecryptionShares>>> BiteEngine::unflattenDecryptionShares(
    const TransactionCiphertextsMap& _txsCiphertexts,
    const ptr<vector<ptr<AESKeyDecryptionShare>>> _allSharesFlattened,
    schain_index _decryptorIndex
) const {

    if (usingRealCrypto()) { // pointer must be set
        CHECK_STATE(_allSharesFlattened);
    }

    auto shares = make_shared<vector<ptr<AESKeyDecryptionShares> > >();
    shares->reserve(_txsCiphertexts.size()); // for number of txs

    size_t globalCiphertextIdx = 0;
    for (auto && [idx, txCiphertexts]: _txsCiphertexts) { // for each tx
        auto decryptSharesForTx = make_shared<AESKeyDecryptionShares>();
        for (size_t i = 0; i < txCiphertexts->count(); ++i) { // for each ciphertext within the tx
            auto currentCiphertextWithinTx = (*txCiphertexts)[i];
            if (usingRealCrypto()) {
                decryptSharesForTx->push_back(_allSharesFlattened->at(globalCiphertextIdx));
            }
            else {
                 decryptSharesForTx->push_back(MockupAESKeyDecryptionShare::mockupDecrypt(currentCiphertextWithinTx, _decryptorIndex));
            }
            globalCiphertextIdx++;
        }
        shares->push_back(decryptSharesForTx);
    }
    return shares;
}
