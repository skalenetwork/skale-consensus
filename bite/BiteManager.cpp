#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/futures/Future.h>
#include <folly/Unit.h>

#include "Log.h"
#include <chains/Schain.h>
#include <crypto/AESKeyDecryptionShare.h>
#include <crypto/AESKeyDecryptionShareSet.h>
#include "crypto/MockupAESKeyDecryptionShare.h"
#include <crypto/AESKeyDecryptionShareList.h>
#include <crypto/MockupAESKeyDecryptionShareSet.h>
#include <crypto/TransactionCiphertexts.h>
#include <algorithm>
#include <string_view>
#include <vector>

#include "BiteCiphertext.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"
#include "datastructures/TransactionCiphertextsMap.h"

#include "rlp/ParsedEthTransaction.h"

#include "BiteManager.h"

#include <crypto/ConsensusAESKeyDecryptionShare.h>
#include <crypto/ConsensusAESKeyDecryptionShareSet.h>
#include <crypto/CryptoManager.h>
#include <crypto/DecryptedAESKeyList.h>
#include <monitoring/LivelinessMonitor.h>

#include "BLSPublicKey.h"
#include "db/TEDecryptionDB.h"
#include "rlp/RLPStream.h"

BiteManager::BiteManager(Schain &_schain) : schain(_schain) {
    doRealCrypto = _schain.getNode()->verifyRealSignatures();
    threadPoolExecutor = std::make_shared<folly::CPUThreadPoolExecutor>(NUM_BITE_VALIDATION_THREADS);
}


ptr<BiteCiphertext> BiteManager::tryGetEncryptedRegularTxFields(
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
        CHECK_STATE(to);

        // compare _to field to BITE magic number
        if (!std::equal(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE,
                        to->begin())) {
            return nullptr;
        }

        auto dataField = ethTx->getTransactionDataField();
        CHECK_STATE(dataField);

        biteCiphertext = ptr<BiteCiphertext>(new BiteCiphertext(dataField, _currentEpochId));
        _transaction->setRegularTxEncryptedData(biteCiphertext); // cache it
    }

    return biteCiphertext;
}

#ifdef BITE2
ptr<std::vector<ptr<BiteCiphertext>>> BiteManager::tryGetEncryptedCATArgs(
            const ptr<Transaction>& _transaction, epoch_id _currentEpochId ) {
    CHECK_STATE(_transaction);

    auto encryptedCATArgs = _transaction->getCATEncryptedArgs();

    // if not cached - try to parse it
    if (!encryptedCATArgs) {

        // if not set bite data already - try parse it
        auto ethTx = _transaction->getAsEthereumTransaction();
        
        auto dataField = ethTx->getTransactionDataField();
        CHECK_STATE(dataField);

        // compare first 4 bytes to BITE2 expected function selector
        if (dataField->size() < BITE_FUNCTION_SELECTOR_SIZE_BYTES || 
            !std::equal(BITE_FUNCTION_SELECTOR_AS_BYTE_ARRAY, BITE_FUNCTION_SELECTOR_AS_BYTE_ARRAY + BITE_FUNCTION_SELECTOR_SIZE_BYTES,
                        dataField->begin())) {
            return nullptr;
        }

        // Parse args as RLP list
        // Data comes as:
        // [funcSelector, RLP( RLP(cipher1, cipher2, ...), RLP(plaintext1, plaintext2, ...) )]

        // offset function selector
        auto dataWithoutSelector = std::vector<uint8_t>(dataField->begin() + BITE_FUNCTION_SELECTOR_SIZE_BYTES, dataField->end());
        RLPItem rlpItem(dataWithoutSelector);
        CHECK_STATE(rlpItem.isList());
        CHECK_STATE(rlpItem.size() == 2); // RLP(ciphertexts, plaintexts)
        RLPItem encryptedArgsRLP = rlpItem[0];
        CHECK_STATE(encryptedArgsRLP.isList());

        encryptedCATArgs = ptr<std::vector<ptr<BiteCiphertext>>>(new std::vector<ptr<BiteCiphertext>>());
        
        encryptedCATArgs->reserve(encryptedArgsRLP.size());
        for (size_t i = 0; i < encryptedArgsRLP.size(); i++) {
            auto argData = make_shared<std::vector<uint8_t>>(encryptedArgsRLP[i].asBytes());
            ptr<BiteCiphertext> biteCiphertext = make_shared<BiteCiphertext>(argData, _currentEpochId);
            encryptedCATArgs->emplace_back( biteCiphertext );
        }
        _transaction->setCATEncryptedArgs(encryptedCATArgs); // cache it   
    }
    return encryptedCATArgs;
}
#endif


void BiteManager::parseBITETransactions(
    ptr<BlockProposal> _proposal) {

    auto encryptedAESKeyMap = make_shared<TransactionCiphertextsMap>();
    auto biteDataFields     = make_shared<std::map<transaction_index, ptr<BiteCiphertext> > >();

    size_t regularTxsStartIdx = 0;
    ptr<vector<ptr<Transaction> > > transactions = _proposal->getTransactionList()->getItems(); 

#ifdef BITE2
    // Try parsing CAT transactions first
    for (size_t i = 0 ; i < transactions->size(); i++) {
        try {
            auto tx = transactions->at(i);
            auto catArgs = tryGetEncryptedCATArgs(tx, _proposal->getEpochID());

            if (catArgs) {
                auto txCiphertexts = make_shared<TransactionCiphertexts>(catArgs);
                encryptedAESKeyMap->emplace(i, txCiphertexts);
            } else {
                // the first non-CAT transaction indicates the start of regular transactions
                regularTxsStartIdx = i;
                break;
            }
        } catch (exception &e) {
            CONS_LOG(err, string("Could not parse CAT transaction:") + e.what());
            _proposal->getFailedTransactionsRef().emplace(i,
                                                          ConnectionSubStatus::CONNECTION_ERROR_CANT_PARSE_PROPOSAL_TRANSACTION);
        }
    }
#endif
                
    // Parse regular txs
    for (size_t i = regularTxsStartIdx; i < transactions->size(); i++) {
        try {
            auto tx = transactions->at(i);
            auto ciphertext = tryGetEncryptedRegularTxFields(tx, _proposal->getEpochID());
            if (ciphertext) {
                auto txCiphertexts = make_shared<TransactionCiphertexts>(ciphertext);
                encryptedAESKeyMap->emplace(i, txCiphertexts);
            }
        } catch (exception &e) {
            CONS_LOG(err, string("Could not parse transaction:") + e.what());
            _proposal->getFailedTransactionsRef().emplace(i,
                                                          ConnectionSubStatus::CONNECTION_ERROR_CANT_PARSE_PROPOSAL_TRANSACTION);
        }
    }


    if (!_proposal->getFailedTransactionsRef().empty()) {
        return;
    }


    _proposal->setTransactionCiphertexts(encryptedAESKeyMap);
}

void BiteManager::callSGXToCreateMyDecryptionSharesForProposalTransactions(
        ptr<BlockProposal> _proposal) {
    MONITOR2(__CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime());

    CHECK_STATE(_proposal);
    // check we are not verifying twice

    auto savedShares = getSchain()->getNode()->getTEDecryptionDB()->getMyDecryptionShares(_proposal->getBlockID(),
                                                                                          _proposal->getProposerIndex());

    if (savedShares) {
        // we already successfully parsed and decrypted shares
        _proposal->setMyDecryptionShares(savedShares);
        return;
    }

    auto transactions = _proposal->getTransactionList()->getItems();

    CHECK_STATE(transactions);

    if (!_proposal->getFailedTransactionsRef().empty()) {
        return;
    }

    CHECK_STATE(_proposal->getTransactionCiphertexts());


    // this function will not throw exception
    auto decryptionShareList = getDecryptionSharesForProposal(_proposal);
    if (!_proposal->getFailedTransactionsRef().empty()) {
        // the block includes invalid transactions, and at this point we know
        // each of them. So we just return them
        return;
    }
    CHECK_STATE(decryptionShareList);
    CHECK_STATE(decryptionShareList->totalCiphertextSharesCount() == _proposal->getTransactionCiphertexts()->totalCiphertextCount());
    // no we know that the decryption shares are valid, we can set them to the proposal
    // now we set the decryption shares list to the block proposal so it is committed to the
    // database when proposal is committed
    _proposal->setMyDecryptionShares(decryptionShareList);

    getSchain()->getNode()->getTEDecryptionDB()->addMyDecryptionShares(decryptionShareList);

}


ptr<AESKeyDecryptionShareList> BiteManager::getDecryptionSharesForProposal(ptr<BlockProposal> _proposal) {
    CHECK_STATE(_proposal)

    MONITOR2(__CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime())

    auto decryptionShareList = make_shared<AESKeyDecryptionShareList>(
            _proposal->getBlockID(),
            _proposal->getProposerIndex(), schain.getSchainIndex());

    ptr<vector<ptr<AESKeyDecryptionShares> > > decryptionSharesVector = getDecryptionSharesFromAESKeys(
            _proposal, schain.getSchainIndex());

    if (!decryptionSharesVector) {
        return nullptr;
    }

    CHECK_STATE(decryptionSharesVector->size() == _proposal->getTransactionCiphertexts()->size());

    auto arrayIndex = 0;
    for (auto &&[txIdx, ciphertexts]: *_proposal->getTransactionCiphertexts()) {
        auto AESKeyDecryptionShare = (*decryptionSharesVector)[arrayIndex];
        decryptionShareList->addShares(txIdx, decryptionSharesVector->at(arrayIndex));
        arrayIndex++;
    }

    return decryptionShareList;
}


ptr<vector<ptr<AESKeyDecryptionShares> > > BiteManager::getDecryptionSharesFromAESKeys(
        ptr<BlockProposal> _proposal, schain_index _decryptorIndex) {
    MONITOR2(__CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime())

    CHECK_STATE(_proposal);

    auto ciphertexts = _proposal->getTransactionCiphertexts();
    CHECK_STATE(ciphertexts);

    auto shares = make_shared<vector<ptr<AESKeyDecryptionShares> > >();
    shares->reserve(ciphertexts->size()); // for number of txs

    // define how to add share depending on real or mockup crypto
    // EncryptedAESKey is the current ciphertext being processed (may be multiple per tx)
    // size_t is the global ciphertext
    std::function<void(ptr<AESKeyDecryptionShares>&, EncryptedAESKey&, size_t)> addShareForCurrentTx;

    if (doRealCrypto) {
        // flatten out vec
        auto sgxAESKeyBatch = _proposal->getSGXAESKeyBatch();

        CHECK_STATE(sgxAESKeyBatch);
        CHECK_STATE(sgxAESKeyBatch->size() == ciphertexts->totalCiphertextCount());

        auto flatDecryptionShares = schain.getCryptoManager()->sgxDecryptAESKeyShareBatch(*sgxAESKeyBatch);
        addShareForCurrentTx = [&](ptr<AESKeyDecryptionShares>& decryptSharesForTx, EncryptedAESKey&, size_t globalCiphertextIdxForCurrTx) {
            decryptSharesForTx->push_back(flatDecryptionShares->at(globalCiphertextIdxForCurrTx));
        };
    } else {
        addShareForCurrentTx = [&](ptr<AESKeyDecryptionShares>& decryptSharesForTx, EncryptedAESKey& ciphertext, size_t) {
            decryptSharesForTx->push_back(MockupAESKeyDecryptionShare::mockupDecrypt(ciphertext, _decryptorIndex));
        }; 
    }

    // unflatten the shares into per-transaction vectors
    size_t globalCiphertextIdx = 0;
    for (auto && [idx, txCiphertexts]: *ciphertexts) { // for each tx
        auto decryptSharesForTx = make_shared<AESKeyDecryptionShares>();
        for (size_t i = 0; i < txCiphertexts->count(); ++i) { // for each ciphertext within the tx
            auto currentCiphertextWithinTx = (*txCiphertexts)[i];
            addShareForCurrentTx(decryptSharesForTx, currentCiphertextWithinTx, globalCiphertextIdx);
            globalCiphertextIdx++;
        }
        shares->push_back(decryptSharesForTx);
    }

    return shares;
}

/**
 * @brief Helper function - appends ciphertexts from TransactionCiphertexts to vector of CipheredKey
 */
void appendCiphertextsToVector(ptr<TransactionCiphertexts> _ciphertexts, std::vector< libBLS::CipheredKey >& _vec, size_t& _ciphertextIdx) {
    if (_ciphertexts->count() > 1) {
        std::vector< libBLS::CipheredKey > cipheredKeysLocal;
        cipheredKeysLocal.reserve(_ciphertexts->count());
        for (const auto& encryptedKey : *_ciphertexts) {
            auto cipheredKey = libBLS::CipheredKey::fromBytes(encryptedKey.data());
            cipheredKeysLocal.push_back(cipheredKey);
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

void BiteManager::computeAndValidateSGXAESKeyBatch(ptr<BlockProposal> _proposal) {
    if (!doRealCrypto) {
        return;
    }

    CHECK_STATE(_proposal);

    ptr<TransactionCiphertextsMap> txsCiphertexts = _proposal->getTransactionCiphertexts();
    auto failedTransactionRef = _proposal->getFailedTransactionsRef();
    auto publicDecryptionValues = make_shared<vector<string>>();

    std::vector< transaction_index > validGlobalIndices; // global indices of all valid txs
    std::vector< libBLS::CipheredKey > cipheredKeys;

    publicDecryptionValues->reserve(txsCiphertexts->totalCiphertextCount());
    cipheredKeys.reserve(txsCiphertexts->totalCiphertextCount());

    for ( auto && it: *txsCiphertexts) {
        size_t failingIdx = 0; // ciphertext idx for each tx starts at 0
        try {
            ptr<TransactionCiphertexts> ciphertexts = it.second;
            CHECK_STATE(ciphertexts)
            
            appendCiphertextsToVector(ciphertexts, cipheredKeys, failingIdx);

            for (size_t i = 0; i < ciphertexts->count(); ++i) {
                validGlobalIndices.push_back(it.first); // repeat global index for each ciphertext
            }
        }
        catch (exception &_e) {
            CONS_LOG(err, fmt::format( "Could not build ciphertext {} of transaction {} from bytes: {}" , failingIdx, static_cast<std::uint32_t>(it.first), _e.what()));
            failedTransactionRef.emplace(it.first,
                                            ConnectionSubStatus::CONNECTION_ERROR_INVALID_AES_KEY_ENCRYPTION_IN_PROPOSAL_TRANSACTION);
        }
    }

    // validate all in parallel
    std::vector< bool > validationResult = libBLS::ThresholdEncryption::validateEncryptionBatchParallel( cipheredKeys );

    // If at least 1 is not valid - mark as failed, log and return
    bool allValid = std::all_of(validationResult.begin(), validationResult.end(),
                                  [](bool v) { return v; });

    // some transactions failed validationgetDecryptionSharesFromAESKeys
    if (!allValid) {
        size_t ciphertextGlobalIdx = 0;
        for (auto& [idx, ciphertexts] : *txsCiphertexts) {
            size_t numCiphertexts = ciphertexts->count();
            for (size_t i = 0; i < numCiphertexts; ++i) {
                if (!validationResult[ciphertextGlobalIdx]) {
                    CONS_LOG(err, fmt::format("AES key encryption validation failed for ciphertext {} of transaction {}", i, (uint32_t)idx));
                    failedTransactionRef.emplace(idx,
                                                    ConnectionSubStatus::CONNECTION_ERROR_INVALID_AES_KEY_ENCRYPTION_IN_PROPOSAL_TRANSACTION);
                }
                ciphertextGlobalIdx++;
            }
        }
        return;
    }

    // convert to string all successful decryption shares
    publicDecryptionValues = make_shared<vector<string>>(
        libBLS::CipheredKey::getDecryptionShareInputBatch( cipheredKeys )
    );

    _proposal->setSGXAESKeyBatch(publicDecryptionValues);
}



ptr<DecryptedRegularTxsMap> BiteManager::verifyAndDecryptTransactionList(
        TransactionList &_transactionList,
        DecryptedAESKeyList &_aesKeys) {

    MONITOR(__CLASS_NAME__, __FUNCTION__);

    auto decryptedFieldsMap = std::make_shared<DecryptedRegularTxsMap>();
    auto catTxsMap          = std::make_shared<DecryptedCATxsMap>();

    auto txs = _transactionList.getItems();
    CHECK_STATE(txs);

    std::vector<folly::Future<folly::Unit>> futures;
    futures.reserve(_aesKeys.getSize());

    std::mutex regularTxMapMutex;
    std::mutex catTxsMapMutex;

    // Helper to avoid repeating folly::via boilerplate
    auto schedule = [&](auto &&fn) {
        futures.emplace_back(
            folly::via(threadPoolExecutor.get(), std::forward<decltype(fn)>(fn))
        );
    };

    try {
        const auto currentEpoch = schain.getNode()->getCurrentEpochId();
        const std::size_t txCount = _transactionList.size();

        for (std::size_t txIdx = 0; txIdx < txCount; ++txIdx) {
            auto tx = txs->at(txIdx);

#ifdef BITE2
        bool allCATsParsed = false;
            // ---------- Try CAT path first ----------
            if (!allCATsParsed) {
                auto encryptedArgs = tryGetEncryptedCATArgs(tx, currentEpoch);
                if (encryptedArgs) {
                    auto decryptedAESKey = _aesKeys.getKeys(txIdx);
                    CHECK_STATE(decryptedAESKey);
                    CHECK_STATE(encryptedArgs->size() == decryptedAESKey->size());

                    try {
                        schedule([this, encryptedArgs, decryptedAESKey, catTxsMap, txIdx, &catTxsMapMutex]() 
                        -> folly::Unit {
                            DecryptedCATArgs decryptedData;
                            decryptedData.args.reserve(encryptedArgs->size());

                            for (std::size_t argIdx = 0; argIdx < encryptedArgs->size(); ++argIdx) {
                                decryptedData.args.push_back(
                                    decryptCiphertext(
                                        encryptedArgs->at(argIdx),
                                        decryptedAESKey->at(argIdx)
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
            auto bite = tryGetEncryptedRegularTxFields(tx, currentEpoch);
            if (bite) {
                auto decryptedAESKey = _aesKeys.getKeys(txIdx);
                CHECK_STATE(decryptedAESKey);
                CHECK_STATE(decryptedAESKey->size() == 1); // single AES key expected

                try {
                    schedule([this, bite, decryptedAESKey, decryptedFieldsMap, txIdx, &regularTxMapMutex]() 
                    -> folly::Unit {
                        auto decryptedTransactionFields =
                            decryptCiphertext(bite, decryptedAESKey->at(0));

                        auto parsedRegularTx =
                            parseDecryptedDataAsRegularTx(decryptedTransactionFields);

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

    return decryptedFieldsMap;  // catTxsMap is still local; decide its final ownership/model
}


DecryptedRegularTxFields BiteManager::parseDecryptedDataAsRegularTx(
        const vector<uint8_t> &_data) const {
    
    CHECK_STATE2(_data.size() >= ADDRESS_SIZE,
                 "Decrypted data is not long enough to include the original tx.to field!");

    RLPItem decryptedDataRlp(_data);
    CHECK_STATE2(decryptedDataRlp.isList(), "Encrypted data rlp size must be a list");
    CHECK_STATE2(decryptedDataRlp.size() == 2,
                 "Encrypted data rlp lsit must have exactly 2 elements");
    // extract decrypted data and to fields
    vector<uint8_t> dataField = decryptedDataRlp[0].asBytes();
    
    std::array<uint8_t, 20> toField;
    std::copy(decryptedDataRlp[1].asBytes().begin(), decryptedDataRlp[1].asBytes().end(), toField.begin());

    auto decryptedFields = DecryptedRegularTxFields {
            .data = std::move(decryptedDataRlp[0].asBytes()),
            .to = std::move(toField),
    };

    return decryptedFields;
}


vector<uint8_t> BiteManager::decryptCiphertext(const ptr<BiteCiphertext> &_bite, DecryptedAESKey &_decryptedAESKey) const {
    CHECK_STATE(_bite);

    vector<uint8_t> decryptedData;

    if (doRealCrypto) {
        auto encryptedData = _bite->getKeyPlusEncryptedData();
        CHECK_STATE(encryptedData != nullptr);

        libBLS::Ciphertext ciphertext = libBLS::Ciphertext::fromBytes(*encryptedData);
        return libBLS::ThresholdEncryption::decrypt(ciphertext, _decryptedAESKey.getAesKey());
    } else {
        auto keyPlusEncryptedData = _bite->getKeyPlusEncryptedData();
        CHECK_STATE(keyPlusEncryptedData);
        return libBLS::ThresholdEncryption::mockupDecrypt(*keyPlusEncryptedData);
    }
}

void BiteManager::corruptFromTimeToTime(shared_ptr<vector<unsigned char> >) {
    static atomic<uint64_t> counter = 0;
    counter++;
    // corrupt AES transactions infrequently
    auto corruptionType = counter % 111;
    if (corruptionType == 1) {
        // introduce some corrupt transactions
        //result->back() = 1;
    } else if (corruptionType == 2) {
        //result->push_back(1);
    } else if (corruptionType == 3) {
        //result->resize(result->size() - 1);
    } else if (corruptionType == 4) {
        // result->front() = 1;
    }
}

ptr<vector<uint8_t> > BiteManager::teEncryptDataAndToAddress(const vector<uint8_t> &_data, const vector<uint8_t> &_to) {
    // RLP encode
    RLPStream stream;
    stream << _data << _to;

    if (this->doRealCrypto) {
        auto [primaryKey, secondaryKey] = schain.getCryptoManager()->getSgxBlsPublicKey();
        CHECK_STATE(primaryKey);
        auto blsKey = primaryKey->getPublicKey();
        libBLS::TEPublicKey teKey(blsKey);

        auto cipherText = libBLS::ThresholdEncryption::encrypt(stream.encode(), teKey);
        auto bytes = std::make_shared<vector<uint8_t>>(cipherText.toBytes());


        corruptFromTimeToTime(bytes);

        return bytes;
    } else {
        return make_shared<vector<uint8_t> >(libBLS::ThresholdEncryption::mockupEncrypt(stream.encode()));
    }
}



// Helper function to split string_view by commas
std::vector<std::string_view> splitByComma(std::string_view s) {
    std::vector<std::string_view> out;

    while (!s.empty()) {
        size_t pos = s.find(',');
        if (pos == std::string_view::npos) {
            out.push_back(s);
            break;
        }
        out.push_back(s.substr(0, pos));
        s.remove_prefix(pos + 1);
    }

    return out;
}


ptr<AESKeyDecryptionShares> BiteManager::createAESDecryptionShares(
        const string& _aesKeyDecryptionShares, schain_index _decryptorIndex, bool _decryptionFailed) {
    
    ptr<AESKeyDecryptionShares> decryptionShares = make_shared<AESKeyDecryptionShares>();
    auto decryptionSharesStrs = splitByComma(_aesKeyDecryptionShares);
    for (const auto& shareStr : decryptionSharesStrs) {
        std::string shareString(shareStr);
        if (doRealCrypto) {
            decryptionShares->push_back(
                make_shared<ConsensusAESKeyDecryptionShare>(
                    shareString, _decryptorIndex, _decryptionFailed));
        } else {
            decryptionShares->push_back(
                make_shared<MockupAESKeyDecryptionShare>(
                    shareString, _decryptorIndex, _decryptionFailed));
        }
    }
    return decryptionShares;
}


ptr<AESKeyDecryptionShareSet> BiteManager::createAESDecryptionShareSet(
        block_id _blockId, transaction_index _transactionIndex, size_t numberOfCiphertexts) {
    if (doRealCrypto) {
        return make_shared<ConsensusAESKeyDecryptionShareSet>(
                _blockId, _transactionIndex, numberOfCiphertexts, schain.getTotalSigners(), schain.getRequiredSigners());
    } else {
        return make_shared<MockupAESKeyDecryptionShareSet>(
                _blockId, _transactionIndex, schain.getTotalSigners(), schain.getRequiredSigners());
    }
}

bool BiteManager::isRealCryptoEnabled() const {
    return doRealCrypto;
}
