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
#include "libBLS/threshold_encryption/ThresholdEncryption.h"
#include "rlp/ParsedEthTransaction.h"

#include "BiteManager.h"

#include <crypto/DecryptedAESKeyList.h>

BiteManager::BiteManager( Schain& _schain ) : schain( _schain ) {
    doRealCrypto = _schain.getNode()->verifyRealSignatures();
}


ConnectionSubStatus BiteManager::verifyAndCreateDecryptionSharesForProposalTransactions(
    const ptr< BlockProposal >& _proposal ) {
    CHECK_STATE( _proposal );
    // check we are not verifying twice
    CHECK_STATE( !_proposal->getMyDecryptionShares() )
    auto transactions = _proposal->getTransactionList()->getItems();

    CHECK_STATE( transactions );


    std::map< transaction_index, ptr< BiteDataField > > biteDataFields;

    try {
        transaction_index index = 0;

        for ( auto& tx : *transactions ) {
            tx->parseAndValidate();
            auto biteDataField = tx->parseAndValidateBiteDataField();
            if (biteDataField) {
                biteDataFields.emplace( index, biteDataField );
            }
            index = index + 1;
        }
    } catch ( exception& e ) {
        LOG( err, string( "Could not parse transaction:" ) + e.what() );
        return ConnectionSubStatus::CONNECTION_ERROR_CANT_PARSE_PROPOSAL_TRANSACTIONS;
    }

    ptr< AESKeyDecryptionShareList > decryptionShareList = nullptr;
    try {
        auto result = decryptBiteDataFields(
            _proposal->getBlockID(), _proposal->getProposerIndex(), biteDataFields );
        auto status = result.second;
        if ( status != ConnectionSubStatus::CONNECTION_OK ) {
            return status;
        }
        decryptionShareList = result.first;
        CHECK_STATE( decryptionShareList );
        CHECK_STATE( decryptionShareList->getSize() == biteDataFields.size() );
    } catch ( exception& e ) {
        LOG( err, string( "Could not decrypt BITE data field" ) + e.what() );
        return ConnectionSubStatus::CONNECTION_ERROR_CANT_DECRYPT_PROPOSAL_TRANSACTIONS;
    }

    // now check if some decryptions failed
    for ( auto iterator : decryptionShareList->getDecryptionShares() ) {
        CHECK_STATE( iterator.second )
        if ( iterator.second->isDecryptionFailed() ) {
            LOG( err, "Decryption failed for transaction:" + to_string( iterator.first ) );
            return ConnectionSubStatus::CONNECTION_ERROR_BLOCK_INCLUDES_INVALID_ENCRYPTIONS;
        }
    }

    // now we set the decryption shares list to the block proposal so it is committed to the
    // database when proposal is committed


    _proposal->setMyDecryptionShares( decryptionShareList );


    return CONNECTION_OK;
}


std::pair< ptr< AESKeyDecryptionShareList >, ConnectionSubStatus >
BiteManager::decryptBiteDataFields( block_id _blockId, schain_index _proposerIndex,
    const std::map< transaction_index, ptr< BiteDataField > >& _biteDataFields ) {
    auto decryptionShareList = make_shared< AESKeyDecryptionShareList >(
        _blockId, _proposerIndex, schain.getSchainIndex() );


    if ( _biteDataFields.empty() ) {
        return { decryptionShareList, ConnectionSubStatus::CONNECTION_OK };
    };

    vector< ptr< BiteDataField > > dataFieldsAsVector;
    dataFieldsAsVector.reserve( _biteDataFields.size() );

    for ( auto&& iterator : _biteDataFields ) {
        dataFieldsAsVector.push_back( iterator.second );
    }

    ptr< vector< ptr< AESKeyDecryptionShare > > > decryptiondSharesVector =
        decryptAESKeys( dataFieldsAsVector );

    if ( !decryptiondSharesVector ) {
        return {
            nullptr, ConnectionSubStatus::CONNECTION_ERROR_CANT_DECRYPT_PROPOSAL_TRANSACTIONS };
    }

    CHECK_STATE( decryptiondSharesVector->size() == _biteDataFields.size() );


    auto arrayIndex = 0;
    for ( auto&& iterator : _biteDataFields ) {
        auto AESKeyDecryptionShare = ( *decryptiondSharesVector )[arrayIndex];
        decryptionShareList->addShare( iterator.first, decryptiondSharesVector->at( arrayIndex ) );
        arrayIndex++;
    }

    decryptionShareList->markComplete();

    return { decryptionShareList, ConnectionSubStatus::CONNECTION_OK };
}


ptr< vector< ptr< AESKeyDecryptionShare > > > BiteManager::decryptAESKeys(
    vector< ptr< BiteDataField > >& _dataFields ) {
    auto result = make_shared< vector< ptr< AESKeyDecryptionShare > > >();
    result->reserve( _dataFields.size() );

    for ( auto&& field : _dataFields ) {
        auto encryptedAESKey = field->getEncryptedAESKey();
        CHECK_STATE( encryptedAESKey );
        auto keyDecryptionShare =
            MockupAESKeyDecryptionShare::mockupDecrypt( encryptedAESKey, schain.getSchainIndex() );
        CHECK_STATE( keyDecryptionShare )
        result->push_back( keyDecryptionShare );
    }

    CHECK_STATE( result->size() == _dataFields.size() );

    return result;
}


ptr< DecryptedTransactions > BiteManager::verifyAndDecryptTransactionList(
    TransactionList& _transactionList, DecryptedAESKeyList& _aesKeys ) {
    auto decryptedTransactions = make_shared< DecryptedTransactions >();

    auto txs = _transactionList.getItems();
    CHECK_STATE( txs );

    try {
        for ( uint64_t i = 0; i < _transactionList.size(); i++ ) {
            auto tx = txs->at( i );
            auto bite = tx->parseAndValidateBiteDataField();
            if ( bite ) {
                CHECK_STATE( _aesKeys.getKey( i ) );

                vector< uint8_t > decryptedOriginalDataField = mockupDecryptDataField( bite );
                // TODO implement actual decryption later

                auto decryptedTransaction =
                    tx->emplaceAndReencodeTransaction( decryptedOriginalDataField );

                decryptedTransactions->emplace( i, decryptedTransaction);
            } else {
                CHECK_STATE( !_aesKeys.getKey( i ) );
            }
        }
    }
    CATCH_LOG_AND_RETHROW_ANY_EXCEPTION( err, "Could not parse BITE transaction" );

    return decryptedTransactions;
}
vector< uint8_t > BiteManager::mockupDecryptDataField( const ptr< BiteDataField >& bite ) const {
    auto keyPlusEncryptedData =  bite->getKeyPlusEncryptedData();
    CHECK_STATE( keyPlusEncryptedData );

    auto decryptedOriginalDataField =
        libBLS::ThresholdEncryption::mockupDecrypt( *keyPlusEncryptedData );
    return decryptedOriginalDataField;
}


ptr< AESKeyDecryptionShare > BiteManager::createAESDecryptionShare(
    const string _aesKeyDecryptionShare, schain_index _decryptorIndex, bool _decryptionFailed ) {
    return make_shared< MockupAESKeyDecryptionShare >(
        _aesKeyDecryptionShare, _decryptorIndex, _decryptionFailed );
}


ptr< AESKeyDecryptionShareSet > BiteManager::createAESDecryptionShareSet(
    block_id _blockId, transaction_index _transactionIndex ) {
    return make_shared< MockupAESKeyDecryptionShareSet >(
        _blockId, _transactionIndex, schain.getTotalSigners(), schain.getRequiredSigners() );
}
