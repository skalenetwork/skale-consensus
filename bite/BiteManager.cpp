#include "Log.h"
#include <chains/Schain.h>
#include <crypto/AESKeyDecryptionShare.h>
#include <crypto/AESKeyDecryptionShareSet.h>
#include "crypto/MockupAESKeyDecryptionShare.h"
#include <crypto/AESKeyDecryptionShareList.h>
#include <crypto/MockupAESKeyDecryptionShareSet.h>

#include "BiteDataFiled.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"

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
#include <future>
#include <thread>
#include <mutex>
#include <chrono>

BiteManager::BiteManager(Schain &_schain) : schain(_schain) {
    doRealCrypto = _schain.getNode()->verifyRealSignatures();
}


void BiteManager::parseBITETransactions(
    ptr<BlockProposal> _proposal) {
    // do simple parsing and validation of BITE format
    // unparsable transactions will be added to failedTransactions
    // transactions starting from the magic number but with incorrect format will be added
    // to failedTransactions
    
    auto transactions = _proposal->getTransactionList()->getItems();
    CHECK_STATE(transactions);
    
    const size_t numTransactions = transactions->size();
    
    auto encryptedAESKeyList = make_shared<EncryptedAESKeyList>();
    auto publicDecryptionValues = make_shared<vector<ptr<string>>>();
    
    // Use parallel processing for large transaction lists
    const size_t PARALLEL_THRESHOLD = 50; // Process in parallel if more than 50 transactions
    
    if (numTransactions <= PARALLEL_THRESHOLD) {
        // Sequential processing for small lists
        transaction_index index = 0;
        for (auto &tx: *transactions) {
            if (!processSingleTransaction(tx, index, _proposal, encryptedAESKeyList, publicDecryptionValues)) {
                return; // Early exit on failure
            }
            index = index + 1;
        }
    } else {
        // Parallel processing for large lists
        auto processingStartTime = std::chrono::high_resolution_clock::now();
        
//        std::mutex encryptedKeysMutex;
//        std::mutex publicValuesMutex;
//        std::mutex failedTransactionsMutex;
        
        const size_t numThreads = 8;
        
        std::vector<std::thread> threads;
        threads.reserve(numThreads);
        
        const size_t chunkSize = (numTransactions + numThreads - 1) / numThreads;
        
        // Thread-local storage for results
        std::vector<EncryptedAESKeyList> threadEncryptedKeys(numThreads);
        std::vector<vector<ptr<string>>> threadPublicValues(numThreads);
        std::vector<map<transaction_index, ConnectionSubStatus>> threadFailedTransactions(numThreads);
        
        for (size_t threadId = 0; threadId < numThreads; ++threadId) {
            size_t startIdx = threadId * chunkSize;
            size_t endIdx = std::min(startIdx + chunkSize, numTransactions);
            
            if (startIdx >= endIdx)
                break;
            
            threads.emplace_back([this, threadId, startIdx, endIdx, &transactions, _proposal, 
                                &threadEncryptedKeys, &threadPublicValues, &threadFailedTransactions]() {
                auto threadStartTime = std::chrono::high_resolution_clock::now();
                size_t processedCount = 0;
                
                for (size_t i = startIdx; i < endIdx; ++i) {
                    try {
                        auto tx = transactions->at(i);
                        tx->parseAndValidate();
                        auto biteDataField = tx->tryGetBiteData(_proposal->getEpochID());
                        
                        if (biteDataField) {
                            threadEncryptedKeys[threadId].emplace(i, biteDataField->getEncryptedAESKey());
                            
                            // Execute core of computeAndValidateSGXAESKeyBatch inline
                            if (doRealCrypto) {
                                try {
                                    auto encryptedAESKey = biteDataField->getEncryptedAESKey();
                                    CHECK_STATE(encryptedAESKey)
                                    auto cipheredKey = libBLS::CipheredKey::fromBytes(*encryptedAESKey->getKey());
                                    auto U = cipheredKey.U;

                                    auto g2AsStringVector = libBLS::ThresholdUtils::G2ToString(U, libBLS::BASE_HEXA);

                                    auto publicDecryptionValue = make_shared<string>();
                                    for (auto const &str: g2AsStringVector) {
                                        publicDecryptionValue->append(str);
                                    }

                                    threadPublicValues[threadId].push_back(publicDecryptionValue);
                                    processedCount++;
                                } catch (exception &_e) {
                                    LOG(err, fmt::format("Could not validate transaction: {} : {}", (uint32_t)i, _e.what()));
                                    threadFailedTransactions[threadId].emplace(i,
                                                        CONNECTION_ERROR_INVALID_AES_KEY_ENCRYPTION_IN_PROPOSAL_TRANSACTION);
                                }
                            } else {
                                processedCount++;
                            }
                        }
                    } catch (exception &e) {
                        LOG(err, fmt::format("Could not parse transaction {}: {}", i, e.what()));
                        threadFailedTransactions[threadId].emplace(i,
                                                                  ConnectionSubStatus::CONNECTION_ERROR_CANT_PARSE_PROPOSAL_TRANSACTION);
                    }
                }
                
                auto threadEndTime = std::chrono::high_resolution_clock::now();
                auto threadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(threadEndTime - threadStartTime);
                
                LOG(info, fmt::format("Parse thread {} processed {} transactions (indices {}-{}) in {} ms (avg: {:.2f} ms per tx)", 
                                      threadId, 
                                      processedCount,
                                      startIdx,
                                      endIdx - 1,
                                      threadDuration.count(),
                                      processedCount > 0 ? static_cast<double>(threadDuration.count()) / processedCount : 0.0));
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Merge results from all threads
        for (size_t threadId = 0; threadId < numThreads; ++threadId) {
            // Merge encrypted keys
            for (auto& entry : threadEncryptedKeys[threadId]) {
                encryptedAESKeyList->emplace(entry.first, entry.second);
            }
            
            // Merge public values
            for (auto& value : threadPublicValues[threadId]) {
                publicDecryptionValues->push_back(value);
            }
            
            // Merge failed transactions
            for (auto& entry : threadFailedTransactions[threadId]) {
                _proposal->getFailedTransactionsRef().emplace(entry.first, entry.second);
            }
        }
        
        auto processingEndTime = std::chrono::high_resolution_clock::now();
        auto processingDuration = std::chrono::duration_cast<std::chrono::milliseconds>(processingEndTime - processingStartTime);
        
        LOG(info, fmt::format("BITE transaction parsing took {} ms for {} transactions (avg: {:.2f} ms per tx)", 
                              processingDuration.count(), 
                              numTransactions,
                              static_cast<double>(processingDuration.count()) / numTransactions));
    }

    if (!_proposal->getFailedTransactionsRef().empty()) {
        return;
    }

    // Set the SGX AES key batch if we have public decryption values
    if (doRealCrypto && !publicDecryptionValues->empty()) {
        _proposal->setSGXAESKeyBatch(publicDecryptionValues);
    }

    _proposal->seAESKeyList(encryptedAESKeyList);
}

bool BiteManager::processSingleTransaction(
    ptr<Transaction> tx, 
    transaction_index index, 
    ptr<BlockProposal> _proposal,
    ptr<EncryptedAESKeyList> encryptedAESKeyList,
    ptr<vector<ptr<string>>> publicDecryptionValues) {
    
    try {
        tx->parseAndValidate();
        auto biteDataField = tx->tryGetBiteData(_proposal->getEpochID());
        if (biteDataField) {
            encryptedAESKeyList->emplace(index, biteDataField->getEncryptedAESKey());
            
            // Execute core of computeAndValidateSGXAESKeyBatch inline
            if (doRealCrypto) {
                try {
                    auto encryptedAESKey = biteDataField->getEncryptedAESKey();
                    CHECK_STATE(encryptedAESKey)
                    auto cipheredKey = libBLS::CipheredKey::fromBytes(*encryptedAESKey->getKey());
                    auto U = cipheredKey.U;

                    auto g2AsStringVector = libBLS::ThresholdUtils::G2ToString(U, libBLS::BASE_HEXA);

                    auto publicDecryptionValue = make_shared<string>();
                    for (auto const &str: g2AsStringVector) {
                        publicDecryptionValue->append(str);
                    }

                    publicDecryptionValues->push_back(publicDecryptionValue);
                } catch (exception &_e) {
                    LOG(err, fmt::format("Could not validate transaction: {} : {}", (uint32_t)index, _e.what()));
                    _proposal->getFailedTransactionsRef().emplace(index,
                                        CONNECTION_ERROR_INVALID_AES_KEY_ENCRYPTION_IN_PROPOSAL_TRANSACTION);
                    return false;
                }
            }
        }
        return true;
    } catch (exception &e) {
        LOG(err, fmt::format("Could not parse transaction {}: {}", (uint32_t)index, e.what()));
        _proposal->getFailedTransactionsRef().emplace(index,
                                                      ConnectionSubStatus::CONNECTION_ERROR_CANT_PARSE_PROPOSAL_TRANSACTION);
        return false;
    }
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

    CHECK_STATE(_proposal->getEncryptedAESKeys());


    // this function will not throw exception
    auto decryptionShareList = getDecryptionSharesFromDataFieldsMap(_proposal);
    if (!_proposal->getFailedTransactionsRef().empty()) {
        // the block includes invalid transactions, and at this point we know
        // each of them. So we just return them
        return;
    }
    CHECK_STATE(decryptionShareList);
    CHECK_STATE(decryptionShareList->getSize() == _proposal->getEncryptedAESKeys()->size());
    // no we know that the decryption shares are valid, we can set them to the proposal
    // now we set the decryption shares list to the block proposal so it is committed to the
    // database when proposal is committed
    _proposal->setMyDecryptionShares(decryptionShareList);

    if (_proposal->getFailedTransactionsRef().empty()) {
        getSchain()->getNode()->getTEDecryptionDB()->addMyDecryptionShares(decryptionShareList);
    }

}


ptr<AESKeyDecryptionShareList> BiteManager::getDecryptionSharesFromDataFieldsMap(ptr<BlockProposal> _proposal) {
    CHECK_STATE(_proposal)

    MONITOR2(__CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime())

    auto decryptionShareList = make_shared<AESKeyDecryptionShareList>(
            _proposal->getBlockID(),
            _proposal->getProposerIndex(), schain.getSchainIndex());


    ptr<vector<ptr<AESKeyDecryptionShare> > > decryptionSharesVector = getDecryptionSharesFromAESKeys(
            _proposal, schain.getSchainIndex());


    if (!decryptionSharesVector) {
        return nullptr;
    }

    CHECK_STATE(decryptionSharesVector->size() == _proposal->getEncryptedAESKeys()->size());


    auto arrayIndex = 0;
    for (auto &&iterator: *_proposal->getEncryptedAESKeys()) {
        auto AESKeyDecryptionShare = (*decryptionSharesVector)[arrayIndex];
        decryptionShareList->addShare(iterator.first, decryptionSharesVector->at(arrayIndex));
        arrayIndex++;
    }


    return decryptionShareList;
}


ptr<vector<ptr<AESKeyDecryptionShare> > > BiteManager::getDecryptionSharesFromAESKeys(
        ptr<BlockProposal> _proposal, schain_index _decryptorIndex) {
    MONITOR2(__CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime())

    CHECK_STATE(_proposal);

    auto encryptedAESKeys = _proposal->getEncryptedAESKeys();
    CHECK_STATE(encryptedAESKeys);



    if (doRealCrypto) {

        auto sgxAESKeyBatch = _proposal->getSGXAESKeyBatch();

        CHECK_STATE(sgxAESKeyBatch);

        CHECK_STATE(sgxAESKeyBatch->size() == encryptedAESKeys->size());

        return schain.getCryptoManager()->sgxDecryptAESKeyShareBatch(*sgxAESKeyBatch);
    } else {
        auto result = make_shared<vector<ptr<AESKeyDecryptionShare> > >();
        for (auto && it: *encryptedAESKeys) {
            result->push_back(MockupAESKeyDecryptionShare::mockupDecrypt(it.second, _decryptorIndex));
        }
        return result;
    }
}

void BiteManager::computeAndValidateSGXAESKeyBatch(ptr<BlockProposal> _proposal) {
    // This function's core logic has been moved to parseBITETransactions for better performance
    // Only keeping basic validation here
    if (!doRealCrypto)
        return;

    CHECK_STATE(_proposal);
    auto encryptedAESKeys = _proposal->getEncryptedAESKeys();
    CHECK_STATE(encryptedAESKeys);
    
    // The actual computation and validation is now done inline in parseBITETransactions
}


ptr<DecryptedTransactionFieldsMap> BiteManager::verifyAndDecryptTransactionList(
        TransactionList &_transactionList, DecryptedAESKeyList &_aesKeys) {

    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    auto decryptedFieldsMap = make_shared<DecryptedTransactionFieldsMap>();
    auto txs = _transactionList.getItems();
    CHECK_STATE(txs);

    try {
        const size_t numTransactions = _transactionList.size();
        
        // Use parallel processing for larger transaction lists
        const size_t PARALLEL_THRESHOLD = 20; // Process in parallel if more than 20 transactions
        
        if (numTransactions <= PARALLEL_THRESHOLD) {
            // Sequential processing for small lists
            for (uint64_t i = 0; i < numTransactions; i++) {
                auto tx = txs->at(i);
                tx->parseAndValidate();
                auto bite = tx->tryGetBiteData(schain.getNode()->getCurrentEpochId());
                if (bite) {
                    auto decryptedAESKey = _aesKeys.getKey(i);
                    CHECK_STATE(decryptedAESKey);

                    try {
                        auto decryptedTransactionFields = decryptFields(bite, *decryptedAESKey);
                        decryptedFieldsMap->emplace(i, decryptedTransactionFields);
                    } catch (const std::exception &e) {
                        LOG(err, fmt::format("Corrupt tx:{} that doesnt decrypt: {}", i, e.what()));
                    }
                } else {
                    CHECK_STATE(!_aesKeys.getKey(i));
                }
            }
        } else {
            // Parallel processing for larger lists
            auto processingStartTime = std::chrono::high_resolution_clock::now();
            
//            std::mutex decryptedMapMutex;
            const size_t numThreads = 8;
            
            std::vector<std::thread> threads;
            threads.reserve(numThreads);
            
            const size_t chunkSize = (numTransactions + numThreads - 1) / numThreads;
            
            // Thread-local storage for results to minimize lock contention
            std::vector<DecryptedTransactionFieldsMap> threadLocalMaps(numThreads);
            
            for (size_t threadId = 0; threadId < numThreads; ++threadId) {
                size_t startIdx = threadId * chunkSize;
                size_t endIdx = std::min(startIdx + chunkSize, numTransactions);
                
                if (startIdx >= endIdx)
                    break;
                
                threads.emplace_back([this, threadId, startIdx, endIdx, &txs, &_aesKeys, &threadLocalMaps]() {
                    auto threadStartTime = std::chrono::high_resolution_clock::now();
                    size_t processedCount = 0;
                    size_t decryptedCount = 0;
                    
                    for (size_t i = startIdx; i < endIdx; ++i) {
                        try {
                            auto tx = txs->at(i);
                            tx->parseAndValidate();
                            auto bite = tx->tryGetBiteData(schain.getNode()->getCurrentEpochId());
                            
                            if (bite) {
                                auto decryptedAESKey = _aesKeys.getKey(i);
                                CHECK_STATE(decryptedAESKey);

                                try {
                                    auto decryptedTransactionFields = decryptFields(bite, *decryptedAESKey);
                                    threadLocalMaps[threadId].emplace(i, decryptedTransactionFields);
                                    decryptedCount++;
                                } catch (const std::exception &e) {
                                    LOG(err, fmt::format("Corrupt tx:{} that doesnt decrypt: {}", i, e.what()));
                                }
                            } else {
                                CHECK_STATE(!_aesKeys.getKey(i));
                            }
                            processedCount++;
                        } catch (const std::exception &e) {
                            LOG(err, fmt::format("Error processing transaction {}: {}", i, e.what()));
                        }
                    }
                    
                    auto threadEndTime = std::chrono::high_resolution_clock::now();
                    auto threadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(threadEndTime - threadStartTime);
                    
                    LOG(info, fmt::format("Decrypt thread {} processed {} transactions, decrypted {} (indices {}-{}) in {} ms (avg: {:.2f} ms per tx)", 
                                          threadId, 
                                          processedCount,
                                          decryptedCount,
                                          startIdx,
                                          endIdx - 1,
                                          threadDuration.count(),
                                          processedCount > 0 ? static_cast<double>(threadDuration.count()) / processedCount : 0.0));
                });
            }
            
            // Wait for all threads to complete
            for (auto& thread : threads) {
                thread.join();
            }
            
            // Merge results from all threads (single-threaded, no locks needed)
            for (size_t threadId = 0; threadId < numThreads; ++threadId) {
                for (auto& entry : threadLocalMaps[threadId]) {
                    decryptedFieldsMap->emplace(entry.first, std::move(entry.second));
                }
            }
            
            auto processingEndTime = std::chrono::high_resolution_clock::now();
            auto processingDuration = std::chrono::duration_cast<std::chrono::milliseconds>(processingEndTime - processingStartTime);
            
            LOG(info, fmt::format("Transaction decryption took {} ms for {} transactions, {} decrypted (avg: {:.2f} ms per tx)", 
                                  processingDuration.count(), 
                                  numTransactions,
                                  decryptedFieldsMap->size(),
                                  static_cast<double>(processingDuration.count()) / numTransactions));
        }
    }
    CATCH_LOG_AND_RETHROW_ANY_EXCEPTION(err, "Could not parse BITE transaction");

    return decryptedFieldsMap;
}

DecryptedTransactionFields
BiteManager::decryptFields(const ptr<BiteDataField> &_bite, DecryptedAESKey &_decryptedAESKey) const {
    CHECK_STATE(_bite);

    ptr<vector<uint8_t> > biteDataField = nullptr;

    if (doRealCrypto) {
        auto encryptedData = _bite->getKeyPlusEncryptedData();
        CHECK_STATE(encryptedData != nullptr);

        // validation is done before submitting data to SGX
        libBLS::Ciphertext ciphertext = libBLS::Ciphertext::fromBytes(*encryptedData, false);
        biteDataField = make_shared<vector<uint8_t>>(
                libBLS::ThresholdEncryption::decrypt(ciphertext, _decryptedAESKey.getAesKey()));
    } else {
        auto keyPlusEncryptedData = _bite->getKeyPlusEncryptedData();
        CHECK_STATE(keyPlusEncryptedData);
        auto decryptedOriginalDataField =
                libBLS::ThresholdEncryption::mockupDecrypt(*keyPlusEncryptedData);
        biteDataField = make_shared<vector<uint8_t> >(decryptedOriginalDataField);
    }

    CHECK_STATE2(biteDataField->size() >= ADDRESS_SIZE,
                 "Decrypted data is not long enough to include the original tx.to field!");

    RLPItem decryptedDataRlp(*biteDataField);
    CHECK_STATE2(decryptedDataRlp.isList(), "Encrypted data rlp size must be a list");
    CHECK_STATE2(decryptedDataRlp.size() == 2,
                 "Encrypted data rlp lsit must have exactly 2 elements");
    // extract decrypted data and to fields
    ptr<vector<uint8_t> > dataField = make_shared<std::vector<uint8_t> >(decryptedDataRlp[0].asBytes());
    ptr<vector<uint8_t> > toField = make_shared<std::vector<uint8_t> >(decryptedDataRlp[1].asBytes());

    auto decryptedFields = DecryptedTransactionFields{
            .data = dataField,
            .to = toField,
    };

    return decryptedFields;
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
        CHECK_STATE(blsKey);
        libBLS::TEPublicKey teKey(*blsKey);

        auto cipherText = libBLS::ThresholdEncryption::encrypt(stream.encode(), teKey);
        auto bytes = std::make_shared<vector<uint8_t>>(cipherText.toBytes());


        corruptFromTimeToTime(bytes);

        return bytes;
    } else {
        return make_shared<vector<uint8_t> >(libBLS::ThresholdEncryption::mockupEncrypt(stream.encode()));
    }
}


ptr<AESKeyDecryptionShare> BiteManager::createAESDecryptionShare(
        const string _aesKeyDecryptionShare, schain_index _decryptorIndex, bool _decryptionFailed) {
    if (doRealCrypto) {
        return make_shared<ConsensusAESKeyDecryptionShare>(
                _aesKeyDecryptionShare, _decryptorIndex, _decryptionFailed);
    } else {
        return make_shared<MockupAESKeyDecryptionShare>(
                _aesKeyDecryptionShare, _decryptorIndex, _decryptionFailed);
    }
}


ptr<AESKeyDecryptionShareSet> BiteManager::createAESDecryptionShareSet(
        block_id _blockId, transaction_index _transactionIndex) {
    if (doRealCrypto) {
        return make_shared<ConsensusAESKeyDecryptionShareSet>(
                _blockId, _transactionIndex, schain.getTotalSigners(), schain.getRequiredSigners());
    } else {
        return make_shared<MockupAESKeyDecryptionShareSet>(
                _blockId, _transactionIndex, schain.getTotalSigners(), schain.getRequiredSigners());
    }
}

bool BiteManager::isRealCryptoEnabled() const {
    return doRealCrypto;
}
