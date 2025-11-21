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
#include <algorithm>

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

BiteManager::BiteManager(Schain &_schain) : schain(_schain) {
    doRealCrypto = _schain.getNode()->verifyRealSignatures();
    threadPoolExecutor = std::make_shared<folly::CPUThreadPoolExecutor>(NUM_BITE_VALIDATION_THREADS);
}

BiteManager::~BiteManager() {
    stopAndDestroyThreadPoolExecutor();
}

void BiteManager::parseBITETransactions(
    ptr<BlockProposal> _proposal) {
    // do simple parsing and validation of BITE format
    // unparsable transactions will be added to failedTransactions
    // transactions starting from the magic number but with incorrect format will be added
    // to failedTransactions
    transaction_index index = 0;

    auto encryptedAESKeyMap = make_shared<EncryptedAESKeyMap>();

    auto biteDataFields = make_shared<std::map<transaction_index, ptr<BiteDataField> > >();

    for (auto &tx: *_proposal->getTransactionList()->getItems()) {
        try {
            tx->parseAndValidate();
            auto biteDataField = tx->tryGetBiteData(_proposal->getEpochID());
            if (biteDataField) {
                encryptedAESKeyMap->emplace(index, biteDataField->getEncryptedAESKey());
            }
            index = index + 1;
        } catch (exception &e) {
            CONS_LOG(err, string("Could not parse transaction:") + e.what());
            _proposal->getFailedTransactionsRef().emplace(index,
                                                          ConnectionSubStatus::CONNECTION_ERROR_CANT_PARSE_PROPOSAL_TRANSACTION);
        }
    }


    if (!_proposal->getFailedTransactionsRef().empty()) {
        return;
    }


    _proposal->setAESKeyMap(encryptedAESKeyMap);
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
    auto decryptionShareList = getDecryptionSharesForProposal(_proposal);
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

    getSchain()->getNode()->getTEDecryptionDB()->addMyDecryptionShares(decryptionShareList);

}


ptr<AESKeyDecryptionShareList> BiteManager::getDecryptionSharesForProposal(ptr<BlockProposal> _proposal) {
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
    if (!doRealCrypto) {
        return;
    }

    CHECK_STATE(_proposal);

    auto encryptedAESKeys = _proposal->getEncryptedAESKeys();
    auto failedTransactionRef = _proposal->getFailedTransactionsRef();
    auto publicDecryptionValues = make_shared<vector<ptr<string>>>();
    publicDecryptionValues->resize(encryptedAESKeys->size());

    std::vector< transaction_index > validGlobalIndices; // global indices of all valid txs
    std::vector< libBLS::CipheredKey > cipheredKeys;
    cipheredKeys.reserve(encryptedAESKeys->size());

    for ( auto && it: *encryptedAESKeys) {
        try {
            auto encryptedAESKey = it.second;
            CHECK_STATE(encryptedAESKey)
            auto cipheredKey = libBLS::CipheredKey::fromBytes(*encryptedAESKey->getKey());
            cipheredKeys.push_back(cipheredKey);
            validGlobalIndices.push_back(it.first);
        }
        catch (exception &_e) {
            CONS_LOG(err, fmt::format( "Could not build transaction from bytes: {} : {}" , static_cast<std::uint32_t>(it.first), _e.what()));
            failedTransactionRef.emplace(it.first,
                                            ConnectionSubStatus::CONNECTION_ERROR_INVALID_AES_KEY_ENCRYPTION_IN_PROPOSAL_TRANSACTION);
        }
    }

    // validate all in parallel
    std::vector< bool > validationResult = libBLS::ThresholdEncryption::validateEncryptionBatchParallel( cipheredKeys );

    // If at least 1 is not valid - mark as failed, log and return
    bool allValid = std::all_of(validationResult.begin(), validationResult.end(),
                                  [](bool v) { return v; });
    if (!allValid) {
        // some transactions failed validation
        for (size_t i = 0; i < validationResult.size(); ++i) {
            if (!validationResult[i]) {
                auto globalIndex = validGlobalIndices[i];
                CONS_LOG(err, fmt::format("AES key encryption validation failed for transaction: {}", static_cast<std::uint32_t>(globalIndex)));
                failedTransactionRef.emplace(globalIndex,
                                                ConnectionSubStatus::CONNECTION_ERROR_INVALID_AES_KEY_ENCRYPTION_IN_PROPOSAL_TRANSACTION);
            }
        }
        return;
    }

    // convert to string all successful decryption shares
    auto decryptionShareInput = libBLS::CipheredKey::getDecryptionShareInputBatch( cipheredKeys );

    // set output decryption shares using global indices from all threads
    for (size_t i = 0; i < cipheredKeys.size(); ++i) {
        publicDecryptionValues->at(i) = make_shared<string>(decryptionShareInput[i]);
    }

    _proposal->setSGXAESKeyBatch(publicDecryptionValues);
}


ptr<DecryptedTransactionFieldsMap> BiteManager::verifyAndDecryptTransactionList(
        TransactionList &_transactionList, DecryptedAESKeyList &_aesKeys) {

    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    auto decryptedFieldsMap = make_shared<DecryptedTransactionFieldsMap>();

    auto txs = _transactionList.getItems();
    CHECK_STATE(txs);

    std::vector<folly::Future<folly::Unit>> futures;
    futures.reserve(_aesKeys.getSize());

    std::mutex mapMutex;

    try {
        for (uint64_t i = 0; i < _transactionList.size(); i++) {
            auto tx = txs->at(i);
            tx->parseAndValidate();
            auto bite = tx->tryGetBiteData(schain.getNode()->getCurrentEpochId());
            if (bite) {
                auto decryptedAESKey = _aesKeys.getKey(i);
                CHECK_STATE(decryptedAESKey);

                try {
                    auto future = folly::via(threadPoolExecutor.get(), [this, bite, decryptedAESKey, &decryptedFieldsMap, i, &mapMutex]() -> folly::Unit {
                        auto decryptedTransactionFields = decryptFields(bite, *decryptedAESKey);
                        std::lock_guard<std::mutex> lock(mapMutex);
                        decryptedFieldsMap->emplace(i, decryptedTransactionFields);
                        return folly::unit;
                    });
                    futures.push_back(std::move(future));
                } catch (const std::exception &e) {
                    CONS_LOG(err, fmt::format("Corrupt tx:{} that doesnt decrypt: {}", i, e.what()));
                }
            } else {
                CHECK_STATE(!_aesKeys.getKey(i));
            }
        }
    }
    CATCH_LOG_AND_RETHROW_ANY_EXCEPTION(err, "Could not parse BITE transaction");
    auto allResults = folly::collectAll(futures).get();

    return decryptedFieldsMap;
}

DecryptedTransactionFields
BiteManager::decryptFields(const ptr<BiteDataField> &_bite, DecryptedAESKey &_decryptedAESKey) const {
    CHECK_STATE(_bite);



    ptr<vector<uint8_t> > biteDataField = nullptr;

    if (doRealCrypto) {
        auto encryptedData = _bite->getKeyPlusEncryptedData();
        CHECK_STATE(encryptedData != nullptr);

        libBLS::Ciphertext ciphertext = libBLS::Ciphertext::fromBytes(*encryptedData);
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
        libBLS::TEPublicKey teKey(blsKey);

        auto cipherText = libBLS::ThresholdEncryption::encrypt(stream.encode(), teKey);
        auto bytes = std::make_shared<vector<uint8_t>>(cipherText.toBytes());


        corruptFromTimeToTime(bytes);

        return bytes;
    } else {
        return make_shared<vector<uint8_t> >(libBLS::ThresholdEncryption::mockupEncrypt(stream.encode()));
    }
}


ptr<AESKeyDecryptionShare> BiteManager::createAESDecryptionShare(
        const string& _aesKeyDecryptionShare, schain_index _decryptorIndex, bool _decryptionFailed) {
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

void BiteManager::stopAndDestroyThreadPoolExecutor() {
    if ( !threadPoolExecutor )
        return;
    threadPoolExecutor->stop();
    threadPoolExecutor->join();
}

bool BiteManager::isRealCryptoEnabled() const {
    return doRealCrypto;
}
