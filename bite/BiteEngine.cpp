#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/futures/Future.h>
#include <folly/Unit.h>

#include "crypto/DecryptedAESKeyList.h"
#include "node/ConsensusTypes.h"
#include "bite/BiteEngine.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"
#include "SkaleCommon.h"
#include "Log.h"


ptr<BiteCiphertext> BiteEngine::tryGetEncryptedRegularTxFields(
            const ptr<Transaction> &_transaction, epoch_id _currentEpochId) const {
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

       biteCiphertext = BiteCodec::tryParseEncryptedRegularTxFields(*to, *data, _currentEpochId);
        _transaction->setRegularTxEncryptedData(biteCiphertext); // cache it
    }

    return biteCiphertext;
}

#ifdef BITE2
ptr<std::vector<ptr<BiteCiphertext>>> BiteEngine::tryGetEncryptedCATArgs(
            const ptr<Transaction>& _transaction, epoch_id _currentEpochId ) const {
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
) const {
    ParseResult result;

    size_t regularTxsStartIdx = 0;
    ptr<vector<ptr<Transaction> > > transactions = txList.getItems(); 

#ifdef BITE2
    // Try parsing CAT transactions first
    for (size_t i = 0 ; i < transactions->size(); i++) {
        try {
            auto tx = transactions->at(i);
            auto catArgs = tryGetEncryptedCATArgs(tx, runtimeCtx.currentEpoch);

            if (catArgs) {
                auto txCiphertexts = make_shared<TransactionCiphertexts>(*catArgs);
                result.txsCiphertexts.emplace(i, txCiphertexts);
            } else {
                // the first non-CAT transaction indicates the start of regular transactions
                regularTxsStartIdx = i;
                break;
            }
        } catch (exception &e) {
            CONS_LOG(err, fmt::format("Could not parse CAT transaction: {}", i) + e.what());
            result.failedTransactions.push_back(i);
        }
    }
#endif

    // Parse regular txs
    for (size_t i = regularTxsStartIdx; i < transactions->size(); i++) {
        try {
            auto tx = transactions->at(i);
            auto ciphertext = tryGetEncryptedRegularTxFields(tx, runtimeCtx.currentEpoch);
            if (ciphertext) {
                auto txCiphertexts = make_shared<TransactionCiphertexts>(ciphertext);
                result.txsCiphertexts.emplace(i, txCiphertexts);
            }
        } catch (exception &e) {
            CONS_LOG(err, fmt::format("Could not parse transaction: {}", i) + e.what());
            result.failedTransactions.push_back(i);
        }
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

    // build CipheredKey vector & identify invalid ones
    for ( auto && [idx, ciphertexts]: txsCiphertexts) {
        size_t failingIdx = 0; // ciphertext idx for each tx starts at 0
        try {
            CHECK_STATE(ciphertexts)
            appendCiphertextsToVector(ciphertexts, cipheredKeys, failingIdx);
        }
        catch (exception &_e) {
            engineResult.invalidCiphertextIndices.push_back(idx);
        }
    }

    BiteCore::CiphertextValidationResult coreResult = core.validateCiphertexts(cipheredKeys);

    // Identify which transactions had invalid ciphertexts
    if (!coreResult.allValid) {
        size_t ciphertextGlobalIdx = 0;
        for (auto& [idx, ciphertexts] : txsCiphertexts) {
            size_t numCiphertexts = ciphertexts->count();
            bool alreadyHasOneInvalidCiphertext = false;
            for (size_t i = 0; i < numCiphertexts; ++i) {
                if (!coreResult.validationResults[ciphertextGlobalIdx] && !alreadyHasOneInvalidCiphertext) {
                    engineResult.invalidCiphertextIndices.push_back(idx);
                    alreadyHasOneInvalidCiphertext = true;
                }
                ciphertextGlobalIdx++;
            }
        }
        return;
    }

    // convert to string all successful decryption shares
    engineResult.publicDecryptionValues =
        std::move(libBLS::CipheredKey::getDecryptionShareInputBatch( cipheredKeys ));
}




DecryptedTransactions BiteEngine::decryptTransactionsListInParallel(
        TransactionList &_transactionList,
        DecryptedAESKeyList &_aesKeys,
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
                auto encryptedArgs = tryGetEncryptedCATArgs(tx, runtimeCtx.currentEpoch);
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
            auto bite = tryGetEncryptedRegularTxFields(tx, runtimeCtx.currentEpoch);
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
    const std::vector<uint8_t>& to
) const {
    auto payload = BiteCodec::encodeRegularTxPayload(plainData, to);
    return core.encryptData(key, payload);      // core uses doRealCrypto internally
}


std::vector<uint8_t> BiteEngine::buildCATData(
    const libBLS::TEPublicKey& key,
    size_t numberOfCiphertexts
) const {
    std::vector<std::vector<uint8_t>> encryptedSerializedArgs;
    encryptedSerializedArgs.reserve(numberOfCiphertexts);

    for (size_t i = 0; i < numberOfCiphertexts; ++i) {
        std::vector<uint8_t> rndData(numberOfCiphertexts * 10);
        auto encryptedData = core.encryptData(key, rndData);

        BiteCiphertext biteCiphertext(
            std::make_shared<std::vector<uint8_t>>(std::move(encryptedData)),
            0 // epoch id not relevant here
        );

        auto serialized = biteCiphertext.getSerializedData();
        CHECK_STATE(serialized);
        encryptedSerializedArgs.push_back(*serialized);
    }

    std::vector<std::vector<uint8_t>> plainArgs;
    const size_t numPlaintexts = numberOfCiphertexts - 1;
    plainArgs.reserve(numPlaintexts);

    for (size_t i = 0; i < numPlaintexts; ++i) {
        plainArgs.emplace_back(numberOfCiphertexts * 5);
    }

    return BiteCodec::encodeCATData(encryptedSerializedArgs, plainArgs);
}
