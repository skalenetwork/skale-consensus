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

BiteManager::BiteManager(Schain &_schain) : schain(_schain) {
    doRealCrypto = _schain.getNode()->verifyRealSignatures();
}


map<transaction_index, ConnectionSubStatus> BiteManager::verifyAndCreateDecryptionSharesForProposalTransactions(
    const ptr<BlockProposal> &_proposal) {

    MONITOR2( __CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime() );


    CHECK_STATE(_proposal);
    // check we are not verifying twice
    CHECK_STATE(!_proposal->getMyDecryptionShares())
    auto transactions = _proposal->getTransactionList()->getItems();

    CHECK_STATE(transactions);

    // this will normally be empty
    map<transaction_index, ConnectionSubStatus> failedTransactions;


    std::map<transaction_index, ptr<BiteDataField> > biteDataFields;
    auto encryptedAESKeyList = make_shared<EncryptedAESKeyList>();


    // do simple parsing and validation of BITE format
    // unparsable transactions will be added to failedTransactions
    // transactions starting from the magic number but with incorrect format will be added
    // to failedTransactions
    transaction_index index = 0;
    for (auto &tx: *transactions) {
        try {
            tx->parseAndValidate();
            auto biteDataField = tx->tryGetBiteData();
            if (biteDataField) {
                biteDataFields.emplace(index, biteDataField);
                encryptedAESKeyList->emplace(index, biteDataField->getEncryptedAESKey());
            }
            index = index + 1;
        } catch (exception &e) {
            LOG(err, string( "Could not parse transaction:" ) + e.what());
            failedTransactions.emplace(index,
                                       ConnectionSubStatus::CONNECTION_ERROR_CANT_PARSE_PROPOSAL_TRANSACTION);
        }
    }

    // this function will not throw exception
    auto decryptionShareList = getDecryptionSharesFromDataFieldsMap(
        _proposal->getBlockID(), _proposal->getProposerIndex(), biteDataFields, failedTransactions);
    if (failedTransactions.size() > 0) {
        // the block includes invalid transactions, and at this point we know
        // each of them. So we just return them
        return failedTransactions;
    }
    CHECK_STATE(decryptionShareList);
    CHECK_STATE(decryptionShareList->getSize() == biteDataFields.size());
    // no we know that the decryption shares are valid, we can set them to the proposal
    // now we set the decryption shares list to the block proposal so it is committed to the
    // database when proposal is committed
    _proposal->setMyDecryptionShares(decryptionShareList, encryptedAESKeyList);

    CHECK_STATE(failedTransactions.empty());

    return failedTransactions;
}


ptr<AESKeyDecryptionShareList> BiteManager::getDecryptionSharesFromDataFieldsMap(
    block_id _blockId, schain_index _proposerIndex,
    const std::map<transaction_index, ptr<BiteDataField> > &
    _biteDataFields, map<transaction_index, ConnectionSubStatus> &_failedTransactions) {

    MONITOR2( __CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime() )


    auto decryptionShareList = make_shared<AESKeyDecryptionShareList>(
        _blockId, _proposerIndex, schain.getSchainIndex());


    if (_biteDataFields.empty()) {
        return decryptionShareList;
    }

    vector<ptr<BiteDataField> > dataFieldsAsVector;
    dataFieldsAsVector.reserve(_biteDataFields.size());

    for (auto &&iterator: _biteDataFields) {
        dataFieldsAsVector.push_back(iterator.second);
    }

    ptr<vector<ptr<AESKeyDecryptionShare> > > decryptiondSharesVector =
            getDecryptionSharesFromDataFields(dataFieldsAsVector, _failedTransactions);

    if (!decryptiondSharesVector) {
        return nullptr;
    }

    CHECK_STATE(decryptiondSharesVector->size() == _biteDataFields.size());


    auto arrayIndex = 0;
    for (auto &&iterator: _biteDataFields) {
        auto AESKeyDecryptionShare = (*decryptiondSharesVector)[arrayIndex];
        decryptionShareList->addShare(iterator.first, decryptiondSharesVector->at(arrayIndex));
        arrayIndex++;
    }


    return decryptionShareList;
}


ptr<vector<ptr<AESKeyDecryptionShare> > > BiteManager::getDecryptionSharesFromAESKeys(
    vector<ptr<EncryptedAESKey> > &_encryptedAESKeys, schain_index _decryptorIndex,  map<transaction_index, ConnectionSubStatus> &_failedTransactions) {

    MONITOR2( __CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime() )

    if (doRealCrypto) {
        vector<ptr<string> > publicDecryptionValuesBatch;

        for (uint64_t i = 0; i< _encryptedAESKeys.size(); i++) {
            try {
                auto encryptedAESKey = _encryptedAESKeys.at(i);
                CHECK_STATE(encryptedAESKey)
                auto cipheredKey = libBLS::CipheredKey::fromBytes(*encryptedAESKey->getKey());
                auto U = cipheredKey.U;
                U.to_affine_coordinates();
                // validate U
                libBLS::ThresholdUtils::validateG2(U);

                auto g2AsStringVector = libBLS::ThresholdUtils::G2ToString(U, libBLS::BASE_HEXA);

                // convert to string
                auto publicDecryptionValue = make_shared<string>();
                for (auto const &str: g2AsStringVector) {
                    publicDecryptionValue->append(str);
                }

                publicDecryptionValuesBatch.push_back(publicDecryptionValue);
            } catch (exception& _e) {
                LOG(err, fmt::format( "Could not validate transaction: {} : {}" , i, _e.what()));
                _failedTransactions.emplace(i,
                ConnectionSubStatus::CONNECTION_ERROR_INVALID_AES_KEY_ENCRYPTION_IN_PROPOSAL_TRANSACTION);
            }
        }

        if (!_failedTransactions.empty()) {
            // found failed transactions, just return
            return nullptr;
        }

        CHECK_STATE(publicDecryptionValuesBatch.size() == _encryptedAESKeys.size())

        return schain.getCryptoManager()->sgxDecryptAESKeyShareBatch(publicDecryptionValuesBatch);
    } else {
        auto result = make_shared<vector<ptr<AESKeyDecryptionShare> > >();
        for (auto &&encryptedAESKey: _encryptedAESKeys) {
            CHECK_STATE(encryptedAESKey);
            result->push_back(MockupAESKeyDecryptionShare::mockupDecrypt(encryptedAESKey, _decryptorIndex));
        }
        return result;
    }
}

ptr<vector<ptr<AESKeyDecryptionShare> > > BiteManager::getDecryptionSharesFromDataFields(
    vector<ptr<BiteDataField> > &_dataFields, map<transaction_index, ConnectionSubStatus> &_failedTransactions) {
    vector<ptr<EncryptedAESKey> > encryptedAESKeys;

    MONITOR2( __CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime() )


    for (auto &&dataField: _dataFields) {
        auto encryptedAESKey = dataField->getEncryptedAESKey();
        CHECK_STATE(encryptedAESKey);
        encryptedAESKeys.push_back(encryptedAESKey);
    }

    auto result = getDecryptionSharesFromAESKeys(encryptedAESKeys, schain.getSchainIndex(),
        _failedTransactions);

    if (result) {
        CHECK_STATE(result->size() == _dataFields.size());
    } else {
        CHECK_STATE(!_failedTransactions.empty())
    }

    return result;
}


ptr<DecryptedTransactionFieldsMap> BiteManager::verifyAndDecryptTransactionList(
    TransactionList &_transactionList, DecryptedAESKeyList &_aesKeys) {
    auto decryptedFieldsMap = make_shared<DecryptedTransactionFieldsMap>();

    auto txs = _transactionList.getItems();
    CHECK_STATE(txs);

    try {
        for (uint64_t i = 0; i < _transactionList.size(); i++) {
            auto tx = txs->at(i);
            tx->parseAndValidate();
            auto bite = tx->tryGetBiteData();
            if (bite) {
                auto decryptedAESKey = _aesKeys.getKey(i);
                CHECK_STATE(decryptedAESKey);

                try {
                    auto decryptedTransactionFields = decryptDataField(bite, *decryptedAESKey);
                    decryptedFieldsMap->emplace(i, decryptedTransactionFields);
                } catch (const std::exception &e) {
                    LOG(err, fmt::format("Corrupt tx:{} that doesnt decrypt: {}", i, e.what()));
                }
            } else {
                CHECK_STATE(!_aesKeys.getKey( i ));
            }
        }
    }
    CATCH_LOG_AND_RETHROW_ANY_EXCEPTION(err, "Could not parse BITE transaction");

    return decryptedFieldsMap;
}

DecryptedTransactionFields
BiteManager::decryptDataField(const ptr<BiteDataField> &_bite, DecryptedAESKey &_decryptedAESKey) const {
    CHECK_STATE(_bite);

    ptr< vector< uint8_t > > dataField = nullptr;

    if (doRealCrypto) {
        auto encryptedData = _bite->getEncryptedData();
        CHECK_STATE(encryptedData != nullptr);

        // TODO - not using ThresholdEncryption functions - this should already be dealt with by libBLS
        // We are also not validating the ciphertext before deciphering (already done in ThresholdEncryption functions)
        std::vector<uint8_t> data = libBLS::ThresholdUtils::aesDecrypt(*encryptedData, _decryptedAESKey.getAesKey());
        CHECK_STATE(data.size() >= BITE_TE_RANDOM_LEN);
        // Strip off the trailing random byte -> libBLS already takes care of this
        dataField = make_shared<vector<uint8_t> >(data.begin(), data.end() - BITE_TE_RANDOM_LEN);
    } else {
        auto keyPlusEncryptedData = _bite->getKeyPlusEncryptedData();
        CHECK_STATE(keyPlusEncryptedData);
        auto decryptedOriginalDataField =
                libBLS::ThresholdEncryption::mockupDecrypt(*keyPlusEncryptedData);
        dataField = make_shared<vector<uint8_t> >(decryptedOriginalDataField);
    }

    CHECK_STATE2(dataField->size() >= ADDRESS_SIZE, "Decrypted data is not long enough to include the original tx.to field!");

    // extract the last 20 bytes from dataField into toField
    ptr< vector< uint8_t > > toField = make_shared< std::vector< uint8_t >>(dataField->end() - ADDRESS_SIZE, dataField->end());
    // remove the to address from dataField
    dataField->erase(dataField->end() - ADDRESS_SIZE, dataField->end());

    auto decryptedFields = DecryptedTransactionFields {
        .data = dataField,
        .to = toField,
    };

    return decryptedFields;
}

void BiteManager::corruptFromTimeToTime(shared_ptr<vector<unsigned char> > result) {
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
        result->front() = 1;
    }
}

ptr<vector<uint8_t> > BiteManager::teEncryptDataAndToAddress(const vector<uint8_t> &_data, const vector<uint8_t> &_to) {
    // copy content of _data, and append _to to end of it
    auto data = _data;
    data.insert(data.end(), _to.begin(), _to.end());

    if (this->doRealCrypto) {
        auto [primaryKey, secondaryKey] = schain.getCryptoManager()->getSgxBlsPublicKey();
        CHECK_STATE(primaryKey);
        auto blsKey = primaryKey->getPublicKey();
        CHECK_STATE(blsKey);
        libBLS::TEPublicKey teKey(*blsKey);

        auto cipherText = libBLS::ThresholdEncryption::encrypt(data, teKey);
        auto bytes = std::make_shared<vector<uint8_t>>(cipherText.toBytes());

        corruptFromTimeToTime(bytes);

        return bytes;
    } else {
        return make_shared<vector<uint8_t> >(libBLS::ThresholdEncryption::mockupEncrypt(data));
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
