/*
    Copyright (C) 2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file TEDecryptionsDB.cpp
    @author Stan Kladko
    @date 2019
*/


#include <oids.h>
#ifdef BITE

#include "TEDecryptionDB.h"

#include "SkaleCommon.h"
#include "Log.h"
#include "thirdparty/json.hpp"
#include "chains/Schain.h"
#include "datastructures/BlockProposal.h"
#include "crypto/DecryptedAESKeyList.h"
#include "crypto/AESKeyDecryptionShareList.h"


#include "leveldb/db.h"
#include "crypto/ThresholdSigShare.h"
#include "crypto/AESKeyDecryptionShareList.h"
#include "LevelDBOptions.h"


#include <bite/BiteManager.h>
#include <bite/BiteAESDecryptionShareSerializer.h>
#include <crypto/AESKeyDecryptionShareSet.h>
#include "TEDecryptionDB.h"


using namespace std;


TEDecryptionDB::TEDecryptionDB(
    Schain* _sChain, string& _dirName, string& _prefix, node_id _nodeId, uint64_t _maxDBSize )
    : CacheLevelDB( _sChain, _dirName, _prefix, _nodeId, _maxDBSize,
          LevelDBOptions::getTEDecryptionDBOptions(), false ) {}

const string& TEDecryptionDB::getFormatVersion() {
    static const string version = "1.0";
    return version;
}


ptr< AESKeyDecryptionShareList > TEDecryptionDB::deserializeDecryptionShareFromString(
    string decryptions ) {
    auto decryptionsVec = std::make_shared< std::vector< uint8_t > >(
        reinterpret_cast< const uint8_t* >( decryptions.data() ),
        reinterpret_cast< const uint8_t* >( decryptions.data() ) + decryptions.size() );
    return BiteAESDecryptionShareSerializer::deserialize(
        decryptionsVec, getSchain()->getCryptoManager(), false );
}



void TEDecryptionDB::addDecryptionShares(
    const ::std::shared_ptr< AESKeyDecryptionShareList >& _decryptionShareList ) {
    CHECK_ARGUMENT( _decryptionShareList )

    auto serializedList = BiteAESDecryptionShareSerializer::serialize( _decryptionShareList );
    CHECK_STATE( serializedList );


    /*    auto decryptionShareListSet = writeByteArrayToSet(
            reinterpret_cast< char* >( serializedList->data() ), serializedList->size(),
            _decryptionShareList->getBlockId(), _decryptionShareList->getDecryptorIndex() );
    */


    WRITE_LOCK(decryptionSetsMutex)

    map< schain_index, ptr< AESKeyDecryptionShareList > >& decryptionShareListSet =
        decryptionsStore[_decryptionShareList->getBlockId()];


    if ( decryptionShareListSet.size() >= requiredSigners ) {
        return;
    }

    if (!decryptionShareListSet.empty()) {
        auto firstShare = decryptionShareListSet.begin()->second;
        CHECK_STATE( firstShare->getProposerIndex() == _decryptionShareList->getProposerIndex() );
        CHECK_STATE( firstShare->getSize() == _decryptionShareList->getSize() );
    }

    decryptionShareListSet[_decryptionShareList->getDecryptorIndex()] = _decryptionShareList;
};

ptr< DecryptedAESKeyList > TEDecryptionDB::mergeAESKeys(block_id _blockId, ptr<EncryptedAESKeyList> _encryptedAESKeyList) {

    CHECK_STATE(_encryptedAESKeyList)

    READ_LOCK(decryptionSetsMutex)

    map< schain_index, ptr< AESKeyDecryptionShareList > >& decryptionShareLists =
        decryptionsStore[_blockId];


    CHECK_STATE( decryptionShareLists.size() >= requiredSigners )


    auto firstDecryptionShareList = decryptionShareLists.begin()->second;
    // TODO - count of decryption shares need to be in DA header
    CHECK_STATE( firstDecryptionShareList );
    auto size = firstDecryptionShareList->getSize();


    for (auto&& it : decryptionShareLists) {
        auto list = it.second;
        CHECK_STATE(list->getSize() == size);
    }


    map< transaction_index, ptr< AESKeyDecryptionShareSet > > decryptionShareSets;


    for ( auto&& decryptionShareIterator : firstDecryptionShareList->getDecryptionShares() ) {
        decryptionShareSets[decryptionShareIterator.first] =
            sChain->getBiteManager()->createAESDecryptionShareSet(
                _blockId, decryptionShareIterator.first );
    }

    for ( auto&& it : decryptionShareLists ) {
        auto decryptionSharesList = it.second;
        CHECK_STATE( decryptionSharesList );
        CHECK_STATE( decryptionSharesList->getProposerIndex() ==
                         firstDecryptionShareList->getProposerIndex() );
        for ( auto&& shareIterator : decryptionSharesList->getDecryptionShares() ) {
            CHECK_STATE( decryptionShareSets.count( shareIterator.first) > 0  );
            auto decryptionSharesSet = decryptionShareSets.at( shareIterator.first );
            decryptionSharesSet->addDecryptionShare( shareIterator.second );
        }
    }


    auto aesKeys = make_shared< DecryptedAESKeyList >();

    for ( auto&& it : decryptionShareSets ) {
        CHECK_STATE( it.second->isEnough() );
        auto key = it.second->verifyAndMergeAESKey(_encryptedAESKeyList->at(it.first));
        CHECK_STATE( key );
        aesKeys->addKey( it.first, *key );
    }

    return aesKeys;
}


void TEDecryptionDB::addMyDecryptionShares(
    const ::std::shared_ptr< AESKeyDecryptionShareList >& _decryptionShareList ) {
    CHECK_ARGUMENT( _decryptionShareList )


    auto serializedList = BiteAESDecryptionShareSerializer::serialize( _decryptionShareList );
    CHECK_STATE( serializedList );


    auto key = createKey( ( _decryptionShareList->getBlockId() ),
                   _decryptionShareList->getProposerIndex() ) +
               ".my";


    writeByteArray( key, serializedList );

    CHECK_STATE( getMyDecryptionShares(
        _decryptionShareList->getBlockId(), _decryptionShareList->getProposerIndex() ) );
}

ptr< AESKeyDecryptionShareList > TEDecryptionDB::getMyDecryptionShares(
    block_id _blockId, schain_index _proposerIndex ) {
    CHECK_STATE( _proposerIndex > 0 );

    auto key = createKey( _blockId, _proposerIndex ) + ".my";

    std::string result = readString( key );

    if ( result.empty() ) {
        return nullptr;
    }

    auto data = std::make_shared< std::vector< uint8_t > >(
        reinterpret_cast< const uint8_t* >( result.data() ),
        reinterpret_cast< const uint8_t* >( result.data() ) + result.size() );

    return BiteAESDecryptionShareSerializer::deserialize(
        data, getSchain()->getCryptoManager(), false );
}


bool TEDecryptionDB::isEnoughDecryptions( block_id _blockID ) {
    READ_LOCK(decryptionSetsMutex)
    return decryptionsStore[_blockID].size() == requiredSigners;
};

bool TEDecryptionDB::isEnoughForeignShares(block_id _blockID) {
    READ_LOCK(decryptionSetsMutex);

    const auto& shares = decryptionsStore[_blockID];
    bool hasOwnShare = shares.find(sChain->getSchainIndex()) != shares.end();

    // if the DB already has the own share, it needs to contain required shares
    // Else, it needs to contain required - 1 shares
    size_t requiredShares = hasOwnShare ? requiredSigners : requiredSigners - 1;
    return shares.size() >= requiredShares;
}


uint64_t TEDecryptionDB::getDecryptionsCount( block_id _blockID ) {
    READ_LOCK(decryptionSetsMutex)
    return decryptionsStore[_blockID].size();
};

#endif
