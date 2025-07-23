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

BiteManager::BiteManager(Schain &_schain) : schain(_schain) {
    doRealCrypto = _schain.getNode()->verifyRealSignatures();
}


void BiteManager::parseBITETransactions(
    ptr<BlockProposal> _proposal, u256 _currentEpochId) {
    // do simple parsing and validation of BITE format
    // unparsable transactions will be added to failedTransactions
    // transactions starting from the magic number but with incorrect format will be added
    // to failedTransactions
    transaction_index index = 0;

    auto encryptedAESKeyList = make_shared<EncryptedAESKeyList>();

    auto biteDataFields = make_shared<std::map<transaction_index, ptr<BiteDataField> > >();

    for (auto &tx: *_proposal->getTransactionList()->getItems()) {
        try {
            tx->parseAndValidate();
            auto biteDataField = tx->tryGetBiteData(_currentEpochId);
            if (biteDataField) {
                biteDataFields->emplace(index, biteDataField);
                encryptedAESKeyList->emplace(index, biteDataField->getEncryptedAESKey());
            }
            index = index + 1;
        } catch (exception &e) {
            LOG(err, string( "Could not parse transaction:" ) + e.what());
            _proposal->getFailedTransactionsRef().emplace(index,
                                                          ConnectionSubStatus::CONNECTION_ERROR_CANT_PARSE_PROPOSAL_TRANSACTION);
        }
    }


    if (!_proposal->getFailedTransactionsRef().empty()) {
        return;
    }


    _proposal->setBiteDataFields(biteDataFields);
    _proposal->seAESKeyList(encryptedAESKeyList);
}

map<transaction_index, ConnectionSubStatus> BiteManager::verifyAndCreateMyDecryptionSharesForProposalTransactions(
    ptr<BlockProposal> _proposal) {
    MONITOR2(__CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime());


    CHECK_STATE(_proposal);
    // check we are not verifying twice


    auto savedShares = getSchain()->getNode()->getTEDecryptionDB()->getMyDecryptionShares(_proposal->getBlockID(),
        _proposal->getProposerIndex());

    if (savedShares) {
        // we already successfully parsed and decrypted shares
        _proposal->setMyDecryptionShares(savedShares);
        return map<transaction_index, ConnectionSubStatus>();
    }


    auto transactions = _proposal->getTransactionList()->getItems();

    CHECK_STATE(transactions);


    parseBITETransactions(_proposal, getSchain()->getNode()->getCurrentEpochId());

    if (!_proposal->getFailedTransactionsRef().empty()) {
        return _proposal->getFailedTransactionsRef();
    }

    CHECK_STATE(_proposal->getBiteDataFields());


    // this function will not throw exception
    auto decryptionShareList = getDecryptionSharesFromDataFieldsMap(_proposal);
    if (!_proposal->getFailedTransactionsRef().empty()) {
        // the block includes invalid transactions, and at this point we know
        // each of them. So we just return them
        return _proposal->getFailedTransactionsRef();
    }
    CHECK_STATE(decryptionShareList);
    CHECK_STATE(decryptionShareList->getSize() == _proposal->getBiteDataFields()->size());
    // no we know that the decryption shares are valid, we can set them to the proposal
    // now we set the decryption shares list to the block proposal so it is committed to the
    // database when proposal is committed
    _proposal->setMyDecryptionShares(decryptionShareList);

    if (_proposal->getFailedTransactionsRef().empty()) {
        getSchain()->getNode()->getTEDecryptionDB()->addMyDecryptionShares(decryptionShareList);
    }

    return _proposal->getFailedTransactionsRef();
}


ptr<AESKeyDecryptionShareList> BiteManager::getDecryptionSharesFromDataFieldsMap(ptr<BlockProposal> _proposal) {
    CHECK_STATE(_proposal)

    MONITOR2(__CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime())

    auto decryptionShareList = make_shared<AESKeyDecryptionShareList>(
        _proposal->getBlockID(),
        _proposal->getProposerIndex(), schain.getSchainIndex());


    auto encryptedAesKeys = _proposal->getEncryptedAESKeys();

    vector<ptr<EncryptedAESKey> > encryptedAESKeysAsAVector;
    encryptedAESKeysAsAVector.reserve(encryptedAesKeys->size());

    for (auto &&iterator: *encryptedAesKeys) {
        encryptedAESKeysAsAVector.push_back(iterator.second);
    }

    ptr<vector<ptr<AESKeyDecryptionShare> > > decryptiondSharesVector = getDecryptionSharesFromAESKeys(
        encryptedAESKeysAsAVector, schain.getSchainIndex(),
        _proposal->getFailedTransactionsRef());


    if (!decryptiondSharesVector) {
        return nullptr;
    }

    CHECK_STATE(decryptiondSharesVector->size() == _proposal->getBiteDataFields()->size());


    auto arrayIndex = 0;
    for (auto &&iterator: *_proposal->getBiteDataFields()) {
        auto AESKeyDecryptionShare = (*decryptiondSharesVector)[arrayIndex];
        decryptionShareList->addShare(iterator.first, decryptiondSharesVector->at(arrayIndex));
        arrayIndex++;
    }


    return decryptionShareList;
}


ptr<vector<ptr<AESKeyDecryptionShare> > > BiteManager::getDecryptionSharesFromAESKeys(
    vector<ptr<EncryptedAESKey> > &_encryptedAESKeys, schain_index _decryptorIndex,
    map<transaction_index, ConnectionSubStatus> &_failedTransactions) {
    MONITOR2(__CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime())

    if (doRealCrypto) {
        vector<ptr<string> > publicDecryptionValuesBatch;

        for (uint64_t i = 0; i < _encryptedAESKeys.size(); i++) {
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
            } catch (exception &_e) {
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


    for (size_t i = 0; i < _dataFields.size(); ++i) {
        auto encryptedAESKey = _dataFields[i]->getEncryptedAESKey();
        auto epochId = _dataFields[i]->getEpoch();
        if ( !encryptedAESKey )
            _failedTransactions.emplace( i, ConnectionSubStatus::CONNECTION_ERROR_INVALID_AES_KEY_ENCRYPTION_IN_PROPOSAL_TRANSACTION );
        else if ( epochId != getSchain()->getNode()->getCurrentEpochId() )
            _failedTransactions.emplace( i, ConnectionSubStatus::CONNECTION_ERROR_INVALID_EPOCH_ID );
        else
            encryptedAESKeys.push_back(encryptedAESKey);
    }

    if ( _failedTransactions.size() )
        return nullptr;

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
                CHECK_STATE(!_aesKeys.getKey( i ));
            }
        }
    }
    CATCH_LOG_AND_RETHROW_ANY_EXCEPTION(err, "Could not parse BITE transaction");

    return decryptedFieldsMap;
}

DecryptedTransactionFields
BiteManager::decryptFields(const ptr<BiteDataField> &_bite, DecryptedAESKey &_decryptedAESKey) const {
    CHECK_STATE(_bite);

    ptr< vector< uint8_t > > biteDataField = nullptr;

    if (doRealCrypto) {
        auto encryptedData = _bite->getKeyPlusEncryptedData();
        CHECK_STATE(encryptedData != nullptr);

        libBLS::Ciphertext ciphertext = libBLS::Ciphertext::fromBytes(*encryptedData);
        biteDataField = make_shared<vector<uint8_t>>(libBLS::ThresholdEncryption::decrypt(ciphertext, _decryptedAESKey.getAesKey()));
    } else {
        auto keyPlusEncryptedData = _bite->getKeyPlusEncryptedData();
        CHECK_STATE(keyPlusEncryptedData);
        auto decryptedOriginalDataField =
                libBLS::ThresholdEncryption::mockupDecrypt(*keyPlusEncryptedData);
        biteDataField = make_shared<vector<uint8_t> >(decryptedOriginalDataField);
    }

    CHECK_STATE2(biteDataField->size() >= ADDRESS_SIZE, "Decrypted data is not long enough to include the original tx.to field!");

    RLPItem decryptedDataRlp( *biteDataField );
    CHECK_STATE2( decryptedDataRlp.isList(), "Encrypted data rlp size must be a list" );
    CHECK_STATE2( decryptedDataRlp.size() == 2,
                  "Encrypted data rlp lsit must have exactly 2 elements" );
    // extract decrypted data and to fields
    ptr< vector< uint8_t > > dataField = make_shared< std::vector< uint8_t > >( decryptedDataRlp[0].asBytes() );
    ptr< vector< uint8_t > > toField = make_shared< std::vector< uint8_t > >( decryptedDataRlp[1].asBytes() );

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
