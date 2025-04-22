#include "libBLS/threshold_encryption/ThresholdEncryption.h"
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

#include "BLSPublicKey.h"

BiteManager::BiteManager(Schain &_schain) : schain(_schain) {
    doRealCrypto = _schain.getNode()->verifyRealSignatures();
}


ConnectionSubStatus BiteManager::verifyAndCreateDecryptionSharesForProposalTransactions(
    const ptr<BlockProposal> &_proposal) {
    CHECK_STATE(_proposal);
    // check we are not verifying twice
    CHECK_STATE(!_proposal->getMyDecryptionShares())
    auto transactions = _proposal->getTransactionList()->getItems();

    CHECK_STATE(transactions);


    std::map<transaction_index, ptr<BiteDataField> > biteDataFields;
    auto encryptedAESKeyList = make_shared<EncryptedAESKeyList>();

    try {
        transaction_index index = 0;

        for (auto &tx: *transactions) {
            tx->parseAndValidate();
            auto biteDataField = tx->parseAndValidateBiteDataField();
            if (biteDataField) {
                biteDataFields.emplace(index, biteDataField);
                encryptedAESKeyList->emplace(index, biteDataField->getEncryptedAESKey());
            }
            index = index + 1;
        }
    } catch (exception &e) {
        LOG(err, string( "Could not parse transaction:" ) + e.what());
        return ConnectionSubStatus::CONNECTION_ERROR_CANT_PARSE_PROPOSAL_TRANSACTIONS;
    }

    ptr<AESKeyDecryptionShareList> decryptionShareList = nullptr;
    try {
        auto result = decryptBiteDataFields(
            _proposal->getBlockID(), _proposal->getProposerIndex(), biteDataFields);
        auto status = result.second;
        if (status != ConnectionSubStatus::CONNECTION_OK) {
            return status;
        }
        decryptionShareList = result.first;
        CHECK_STATE(decryptionShareList);
        CHECK_STATE(decryptionShareList->getSize() == biteDataFields.size());
    } catch (exception &e) {
        LOG(err, string( "Could not decrypt BITE data field:" ) + e.what());
        return ConnectionSubStatus::CONNECTION_ERROR_CANT_DECRYPT_PROPOSAL_TRANSACTIONS;
    }

    // now check if some decryptions failed
    for (auto iterator: decryptionShareList->getDecryptionShares()) {
        CHECK_STATE(iterator.second)
        if (iterator.second->isDecryptionFailed()) {
            LOG(err, "Decryption failed for transaction:" + to_string( iterator.first ));
            return ConnectionSubStatus::CONNECTION_ERROR_BLOCK_INCLUDES_INVALID_ENCRYPTIONS;
        }
    }

    // now we set the decryption shares list to the block proposal so it is committed to the
    // database when proposal is committed


    _proposal->setMyDecryptionShares(decryptionShareList, encryptedAESKeyList);


    return CONNECTION_OK;
}


std::pair<ptr<AESKeyDecryptionShareList>, ConnectionSubStatus>
BiteManager::decryptBiteDataFields(block_id _blockId, schain_index _proposerIndex,
                                   const std::map<transaction_index, ptr<BiteDataField> > &_biteDataFields) {
    auto decryptionShareList = make_shared<AESKeyDecryptionShareList>(
        _blockId, _proposerIndex, schain.getSchainIndex());


    if (_biteDataFields.empty()) {
        return {decryptionShareList, ConnectionSubStatus::CONNECTION_OK};
    };

    vector<ptr<BiteDataField> > dataFieldsAsVector;
    dataFieldsAsVector.reserve(_biteDataFields.size());

    for (auto &&iterator: _biteDataFields) {
        dataFieldsAsVector.push_back(iterator.second);
    }

    ptr<vector<ptr<AESKeyDecryptionShare> > > decryptiondSharesVector =
            getDecryptionSharesFromDataFields(dataFieldsAsVector);

    if (!decryptiondSharesVector) {
        return {
            nullptr, ConnectionSubStatus::CONNECTION_ERROR_CANT_DECRYPT_PROPOSAL_TRANSACTIONS
        };
    }

    CHECK_STATE(decryptiondSharesVector->size() == _biteDataFields.size());


    auto arrayIndex = 0;
    for (auto &&iterator: _biteDataFields) {
        auto AESKeyDecryptionShare = (*decryptiondSharesVector)[arrayIndex];
        decryptionShareList->addShare(iterator.first, decryptiondSharesVector->at(arrayIndex));
        arrayIndex++;
    }

    return {decryptionShareList, ConnectionSubStatus::CONNECTION_OK};
}


ptr<vector<ptr<AESKeyDecryptionShare> > > BiteManager::getDecryptionSharesFromAESKeys(
    vector<ptr<EncryptedAESKey> > &_encryptedAESKeys, schain_index _decryptorIndex) {
    if (doRealCrypto) {
        vector<ptr<string> > publicDecryptionValuesBatch;

        for (auto &&encryptedAESKey: _encryptedAESKeys) {
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
    vector<ptr<BiteDataField> > &_dataFields) {
    vector<ptr<EncryptedAESKey> > encryptedAESKeys;

    for (auto &&dataField: _dataFields) {
        auto encryptedAESKey = dataField->getEncryptedAESKey();
        CHECK_STATE(encryptedAESKey);
        encryptedAESKeys.push_back(encryptedAESKey);
    }

    auto result = getDecryptionSharesFromAESKeys(encryptedAESKeys, schain.getSchainIndex());

    CHECK_STATE(result);

    CHECK_STATE(result->size() == _dataFields.size());

    return result;
}


ptr<DecryptedTransactionDataFields> BiteManager::verifyAndDecryptTransactionList(
    TransactionList &_transactionList, DecryptedAESKeyList &_aesKeys) {
    auto decryptedDataFields = make_shared<DecryptedTransactionDataFields>();

    auto txs = _transactionList.getItems();
    CHECK_STATE(txs);

    try {
        for (uint64_t i = 0; i < _transactionList.size(); i++) {
            auto tx = txs->at(i);
            auto bite = tx->parseAndValidateBiteDataField();
            if (bite) {
                auto decryptedAESKey = _aesKeys.getKey(i);
                CHECK_STATE(decryptedAESKey);

                ptr<vector<uint8_t> > decryptedOriginalDataField = nullptr;

                try {
                    decryptedOriginalDataField = decryptDataField(bite, *decryptedAESKey);
                } catch (const std::exception &e) {
                    LOG(err, fmt::format("Corrupt tx:{} that doesnt decrypt: {}"
                            " skip from block, TODO: charge user min gas fee penalty",
                            i, e.what()));
                    decryptedDataFields->emplace(i, nullptr);;
                }

                decryptedDataFields->emplace(i, decryptedOriginalDataField);
            } else {
                CHECK_STATE(!_aesKeys.getKey( i ));
            }
        }
    }
    CATCH_LOG_AND_RETHROW_ANY_EXCEPTION(err, "Could not parse BITE transaction");

    return decryptedDataFields;
}

ptr<vector<uint8_t> >
BiteManager::decryptDataField(const ptr<BiteDataField> &_bite, DecryptedAESKey &_decryptedAESKey) const {
    CHECK_STATE(_bite);

    if (doRealCrypto) {
        auto encryptedData = _bite->getEncryptedData();
        CHECK_STATE(encryptedData != nullptr);

        std::vector<uint8_t> data = libBLS::ThresholdUtils::aesDecrypt(*encryptedData, _decryptedAESKey.getAesKey());
        CHECK_STATE(data.size() >= BITE_TE_RANDOM_LEN);
        // Strip off the trailing random byte
        return make_shared<vector<uint8_t> >(data.begin(), data.end() - BITE_TE_RANDOM_LEN);
    } else {
        auto keyPlusEncryptedData = _bite->getKeyPlusEncryptedData();
        CHECK_STATE(keyPlusEncryptedData);
        auto decryptedOriginalDataField =
                libBLS::ThresholdEncryption::mockupDecrypt(*keyPlusEncryptedData);
        return make_shared<vector<uint8_t> >(decryptedOriginalDataField);
    }
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

ptr<vector<uint8_t> > BiteManager::teEncryptData(const vector<uint8_t> &_data) {
    if (this->doRealCrypto) {
        auto [primaryKey, secondaryKey] = schain.getCryptoManager()->getSgxBlsPublicKey();
        CHECK_STATE(primaryKey);
        auto blsKey = primaryKey->getPublicKey();
        CHECK_STATE(blsKey);
        libBLS::TEPublicKey teKey(*blsKey);
        auto cipherText = libBLS::ThresholdEncryption::encrypt(_data, teKey);
        CHECK_STATE(cipherText.data);
        auto encodedCipheredKey = cipherText.key.toBytes();
        auto result = make_shared<vector<uint8_t> >(encodedCipheredKey.begin(), encodedCipheredKey.end());
        result->insert(result->end(), cipherText.data->begin(), cipherText.data->end());
        CHECK_STATE(result->size() == encodedCipheredKey.size() + cipherText.data->size());


        corruptFromTimeToTime(result);


        return result;
    } else {
        return make_shared<vector<uint8_t> >(libBLS::ThresholdEncryption::mockupEncrypt(_data));
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
