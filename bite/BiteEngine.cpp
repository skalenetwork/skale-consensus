#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/futures/Future.h>
#include <folly/Unit.h>

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

#ifdef BITE2
ptr<std::vector<ptr<BiteCiphertext>>> BiteEngine::tryGetEncryptedCATArgs(
            const ptr<Transaction>& _transaction, epoch_id _currentEpochId ) {
    CHECK_STATE(_transaction);

    auto encryptedCATArgs = _transaction->getCATEncryptedArgs();

    // if not cached - try to parse it
    if (!encryptedCATArgs) {

        // if not set bite data already - try parse it
        auto ethTx = _transaction->getAsEthereumTransaction();
        
        auto dataField = ethTx->getTransactionDataField();
        CHECK_STATE(dataField);

        encryptedCATArgs = BiteCodec::tryParseEncryptedCATArgs(*dataField, _currentEpochId);
        _transaction->setCATEncryptedArgs(encryptedCATArgs); // cache it   
    }
    return encryptedCATArgs;
}
#endif

BiteEngine::ParseResult BiteEngine::parseAndCacheBITETransactions(
    const TransactionList& txList,
    BiteRuntimeContext& runtimeCtx
) {
    ParseResult result;

    ptr<vector<ptr<Transaction> > > transactions = txList.getItems();
    // Assume no regular txs by default; only process regular txs if a non-CAT is found
    size_t regularTxsStartIdx = transactions->size();

    std::set<size_t> failedTxIndices;

#ifdef BITE2
    // Try parsing CAT transactions first
    for (size_t i = 0 ; i < transactions->size(); i++) {
        try {
            auto tx = transactions->at(i);
            auto catArgs = BiteEngine::tryGetEncryptedCATArgs(tx, runtimeCtx.currentEpoch);

            if (catArgs) {
                auto txCiphertexts = make_shared<TransactionCiphertexts>(*catArgs);
                result.txsCiphertexts.emplace(i, txCiphertexts);
            } else {
                // the first non-CAT transaction indicates the start of regular transactions
                regularTxsStartIdx = i;
                break;
            }
        } catch (exception &e) {
            CONS_LOG(err, fmt::format("Could not try to parse as CAT transaction: {}: ", i) + e.what());
            failedTxIndices.insert(i);
        }
    }
#else
    // No BITE2 means all txs are regular
    regularTxsStartIdx = 0;
#endif

    // Parse regular txs
    for (size_t i = regularTxsStartIdx; i < transactions->size(); i++) {
        try {
            if (failedTxIndices.find(i) != failedTxIndices.end()) {
                // already reported failure for this tx (e.g., CAT parsing failed)
                // no need to try parsing again
                continue;
            }
            auto tx = transactions->at(i);
            auto ciphertext = BiteEngine::tryGetEncryptedRegularTxFields(tx, runtimeCtx.currentEpoch);
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


/**
 * @brief Helper function - appends ciphertexts from TransactionCiphertexts to vector of CipheredKey
 */
void appendCiphertextsToVector(ptr<TransactionCiphertexts> _ciphertexts, std::vector< libBLS::CipheredKey >& _vec, size_t& _ciphertextIdx) {
    if (_ciphertexts->count() > 1) {
        std::vector< libBLS::CipheredKey > cipheredKeysLocal;
        cipheredKeysLocal.reserve(_ciphertexts->count());
        for (const auto& encryptedKey : *_ciphertexts) {
            cipheredKeysLocal.emplace_back(libBLS::CipheredKey::fromBytes(encryptedKey.data()));
            _ciphertextIdx++;
        }
        // Only insert all after all have been successfully created (there could be exceptions thrown)
        _vec.insert(_vec.end(), cipheredKeysLocal.begin(), cipheredKeysLocal.end());
    }
    else {
        auto cipheredKey = libBLS::CipheredKey::fromBytes((*_ciphertexts)[0].data());
        _vec.push_back(cipheredKey);
    }
}


BiteEngine::CiphertextValidationResult BiteEngine::validateCiphertexts(const TransactionCiphertextsMap& txsCiphertexts) const {
    CiphertextValidationResult engineResult;

    std::vector< libBLS::CipheredKey > cipheredKeys;
    cipheredKeys.reserve(txsCiphertexts.totalCiphertextCount());

    // Track only transactions that successfully parsed, preserving order with cipheredKeys
    std::vector<std::pair<transaction_index, size_t>> parsedTxs;
    size_t expectedValidationResults = 0;

    // build CipheredKey vector & identify invalid ones
    for ( auto && [idx, ciphertexts]: txsCiphertexts) {
        size_t failingIdx = 0; // ciphertext idx for each tx starts at 0
        try {
            CHECK_STATE(ciphertexts)
            appendCiphertextsToVector(ciphertexts, cipheredKeys, failingIdx);
            parsedTxs.emplace_back(idx, ciphertexts->count());
            expectedValidationResults += ciphertexts->count();
        }
        catch (exception &_e) {
            engineResult.invalidCiphertextIndices.push_back(idx);
            engineResult.failureReasons.push_back("Ciphertext with index " + std::to_string(failingIdx) + " failed to parse: " + std::string(_e.what()));
        }
    }

    // From the successfully built CipheredKey vector, validate ciphertexts
    BiteCore::CiphertextValidationResult coreResult = core.validateCiphertexts(cipheredKeys);
    CHECK_STATE(coreResult.validationResults.size() == expectedValidationResults);

    if (!coreResult.allValid) {
        size_t ciphertextGlobalIdx = 0;
        for (auto& [idx, numCiphertexts] : parsedTxs) {
            bool alreadyHasOneInvalidCiphertext = false;
            for (size_t i = 0; i < numCiphertexts; ++i) {
                if (!coreResult.validationResults[ciphertextGlobalIdx] && !alreadyHasOneInvalidCiphertext) {
                    engineResult.invalidCiphertextIndices.push_back(idx);
                    engineResult.failureReasons.push_back("Ciphertext with global index " + std::to_string(ciphertextGlobalIdx) + " failed semantic validation");
                    alreadyHasOneInvalidCiphertext = true;
                }
                ciphertextGlobalIdx++;
            }
        }
    }

    // Only add public decryption values if both
    // 1) ciphertext parsing was successful
    // 2) core validation marked all as valid
    if (engineResult.allValid() && coreResult.allValid) {
        // convert to string all successful decryption shares
        engineResult.publicDecryptionValues =
            libBLS::CipheredKey::getDecryptionShareInputBatch( cipheredKeys );
    }

    return engineResult;
}

std::shared_ptr<DecryptedAESKeyList> BiteEngine::mergeAESKeys(
    block_id _blockId,
    TransactionCiphertextsMap& _txCiphertexts,
    const std::map<schain_index, std::shared_ptr<AESKeyDecryptionShareList>>& _decryptionShareMap,
    const std::vector<libBLS::TEPublicKeyShare>& _tePublicKeyShares,
    const BiteRuntimeContext& _runtimeCtx
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

    // 1 per transaction (but each tx may contain multiple ciphertexts)
    std::map<transaction_index, ptr<AESKeyDecryptionShareSet>> decryptionShareSets;
    std::map<transaction_index, std::vector<libBLS::CipheredKey>> encryptions;

    bool toValidate = false;
    for (auto&& [idx, shares] : firstDecryptionShareList->getDecryptionShares()) {
        decryptionShareSets[idx] =
            createAESDecryptionShareSetObject(_blockId, idx, shares->size());

        if (config.sgxEnabled) {
            auto& ciphertextsForCurrTx = *_txCiphertexts.at(idx);
            for (auto& ciphertext : ciphertextsForCurrTx) {
                encryptions[idx].push_back(
                    libBLS::CipheredKey::fromBytes(ciphertext.data(), toValidate));
            }
        }
    }

    std::vector<folly::Future<folly::Unit>> futures;
    futures.reserve(decryptionShareSets.size());

    auto aesKeys = make_shared< DecryptedAESKeyList >();
    std::mutex aesKeysMutex;

    auto processTx = [&](transaction_index txId, ptr<AESKeyDecryptionShareSet> decryptionSet) -> folly::Unit {
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

                for (size_t ciphertextId = 0; ciphertextId < numberOfCiphertexts; ++ciphertextId) {
                    std::vector<libBLS::CipheredKey> cipheredKeys{ encryptions.at(txId).at(ciphertextId) };

                    auto result = libBLS::ThresholdEncryption::validateDecryptionSharesBatch(
                            cipheredKeys, teShares.at(ciphertextId), publicKeys.at(ciphertextId));

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
            std::lock_guard<std::mutex> lock(aesKeysMutex);
            aesKeys->addKeys( txId, *keys );
        }
        return folly::unit;
    };

    if (_runtimeCtx.threadPoolExecutor) {
        for ( auto&& [txId, decryptionSet]: decryptionShareSets ) {
            auto txIdLocal = txId;
            auto decryptionSetLocal = decryptionSet;
            futures.emplace_back(folly::via(_runtimeCtx.threadPoolExecutor.get(), [&, txIdLocal, decryptionSetLocal]() {
                return processTx(txIdLocal, decryptionSetLocal);
            }));
        }
        folly::collectAll(futures).get();
    } else {
        for ( auto&& [txId, decryptionSet]: decryptionShareSets ) {
            processTx(txId, decryptionSet);
        }
    }

    CHECK_STATE2(aesKeys->totalDecryptedCiphertextsCount() == _txCiphertexts.totalCiphertextCount(),
        "Not all aes keys could be decrypted");
    return aesKeys;
}




DecryptedTransactions BiteEngine::decryptTransactionsListInParallel(
        const TransactionList &_transactionList,
        const DecryptedAESKeyList &_aesKeys,
        BiteRuntimeContext& runtimeCtx
) const {
 
    auto decryptedFieldsMap = std::make_shared<DecryptedRegularTxsMap>();
    std::mutex regularTxMapMutex;
    
#ifdef BITE2
    auto catTxsMap          = std::make_shared<DecryptedCATxsMap>();
    std::mutex catTxsMapMutex;
#endif

    auto txs = _transactionList.getItems();
    CHECK_STATE(txs);

    std::vector<folly::Future<folly::Unit>> futures;
    futures.reserve(_aesKeys.getSize());



    // Helper to avoid repeating folly::via boilerplate
    auto schedule = [&](auto &&fn) {
        futures.emplace_back(
            folly::via(runtimeCtx.threadPoolExecutor.get(), std::forward<decltype(fn)>(fn))
        );
    };

    try {
        const std::size_t txCount = _transactionList.size();

#ifdef BITE2
        bool allCATsParsed = false;
#endif

        for (std::size_t txIdx = 0; txIdx < txCount; ++txIdx) {
            auto tx = txs->at(txIdx);

#ifdef BITE2
            // ---------- Try CAT path first ----------
            if (!allCATsParsed) {
                ptr< std::vector<ptr<BiteCiphertext> > > encryptedArgs;
                try { 
                    encryptedArgs = tryGetEncryptedCATArgs(tx, runtimeCtx.currentEpoch);
                } catch (const std::exception& e) {
                    CONS_LOG(warn, fmt::format("Could not try to parse as CAT encrypted transaction during decryption: {}: {}", txIdx, e.what()));
                }
                if (encryptedArgs) {
                    auto decryptedAESKey = _aesKeys.getKeys(txIdx);
                    CHECK_STATE(decryptedAESKey);
                    CHECK_STATE(encryptedArgs->size() == decryptedAESKey->size());

                    try {
                        schedule([corePtr = &this->core, encryptedArgs, decryptedAESKey, catTxsMap, txIdx, &catTxsMapMutex]() 
                        -> folly::Unit {
                            DecryptedCATArgs decryptedData;
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

                            std::lock_guard<std::mutex> lock(catTxsMapMutex);
                            catTxsMap->emplace(txIdx, std::move(decryptedData));

                            return folly::unit;
                        });
                    } catch (const std::exception &e) {
                        CONS_LOG(err, fmt::format("Corrupt CAT tx:{} that doesn't decrypt: {}", txIdx, e.what()));
                        std::lock_guard<std::mutex> lock(catTxsMapMutex);
                        catTxsMap->emplace(txIdx, std::nullopt);
                    }

                    // This tx is CAT, do not treat it as regular
                    continue;
                }
                else {
                    // No more CAT transactions expected after the first non-CAT one
                    allCATsParsed = true;
                }
            }

#endif // BITE2

            // ---------- Regular BITE1-style encryption ----------
            ptr<BiteCiphertext> bite;
            try {
                bite = tryGetEncryptedRegularTxFields(tx, runtimeCtx.currentEpoch);
            } catch (const std::exception& e) {
                CONS_LOG(warn, fmt::format("Could not try to parse as regular encrypted transaction during decryption: {}: {}", txIdx, e.what()));
            }

            if (bite) {
                auto decryptedAESKey = _aesKeys.getKeys(txIdx);
                CHECK_STATE(decryptedAESKey);
                CHECK_STATE(decryptedAESKey->size() == 1); // single AES key expected

                try {
                    schedule([corePtr = &this->core, bite, decryptedAESKey, decryptedFieldsMap, txIdx, &regularTxMapMutex]() 
                    -> folly::Unit {
                        auto decryptedTransactionFields = BiteCodec::decryptCiphertext(*bite, 
                                decryptedAESKey->at(0).getAesKey(), 
                                *corePtr
                            );

                        auto parsedRegularTx =
                            BiteCodec::parseRegularTxDecryptedData(decryptedTransactionFields);

                        std::lock_guard<std::mutex> lock(regularTxMapMutex);
                        decryptedFieldsMap->emplace(txIdx, std::move(parsedRegularTx));

                        return folly::unit;
                    });
                } catch (const std::exception &e) {
                    CONS_LOG(err, fmt::format("Corrupt regular tx:{} that doesn't decrypt: {}", txIdx, e.what()));
                    std::lock_guard<std::mutex> lock(regularTxMapMutex);
                    decryptedFieldsMap->emplace(txIdx, std::nullopt);
                }

                continue;
            }

            // ---------- No BITE data at all ----------
            // If it's neither CAT nor regular encrypted, we must not have AES keys
            CHECK_STATE(!_aesKeys.getKeys(txIdx));
        }
    }
    CATCH_LOG_AND_RETHROW_ANY_EXCEPTION(err, "Could not parse BITE transaction");

    folly::collectAll(futures).get();

    return DecryptedTransactions(
#ifdef BITE2 
        catTxsMap,
#endif 
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


std::vector<uint8_t> BiteEngine::buildCATData(
    const libBLS::TEPublicKey& key,
    size_t numberOfCiphertexts,
    uint64_t epochId
) const {
    std::vector<std::vector<uint8_t>> encryptedSerializedArgs;
    encryptedSerializedArgs.reserve(numberOfCiphertexts);

    for (size_t i = 0; i < numberOfCiphertexts; ++i) {
        std::vector<uint8_t> rndData(numberOfCiphertexts * 10);
        auto encryptedData = core.encryptData(key, rndData);

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

    return BiteCodec::encodeCATData(encryptedSerializedArgs, plainArgs);
}


std::shared_ptr<AESKeyDecryptionShares> BiteEngine::createDecryptionSharesObjects(
    const std::vector<std::string_view>& shareStrs,
    schain_index decryptorIndex,
    bool decryptionFailed
) const {
    auto shares = std::make_shared<AESKeyDecryptionShares>();
    for (auto shareStr : shareStrs) {
        std::string s(shareStr);
        if (usingRealCrypto()) {
            shares->push_back(
                std::make_shared<ConsensusAESKeyDecryptionShare>(
                    s, decryptorIndex, decryptionFailed));
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
