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

#ifdef BITE

#include "TEDecryptionDB.h"

#include "SkaleCommon.h"
#include "Log.h"
#include "thirdparty/json.hpp"
#include "chains/Schain.h"
#include "datastructures/BlockProposal.h"
#include "crypto/DecryptedAESKeyList.h"
#include "crypto/AESKeyDecryptionShareList.h"

#include "node/Node.h"
#include "leveldb/db.h"
#include "crypto/ThresholdSigShare.h"
#include "LevelDBOptions.h"


#include <bite/BiteManager.h>
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


bool TEDecryptionDB::haveDecryptions( const ptr< BlockProposal >& _proposal ) {
    CHECK_ARGUMENT( _proposal )
    return keyExistsInSet( _proposal->getBlockID(), _proposal->getProposerIndex() );
}


ptr<AESKeyDecryptionShareList> TEDecryptionDB::getDecryptions( block_id _blockId, schain_index _decryptorIndex ) {
    return make_shared<AESKeyDecryptionShareList>(_blockId, 1, _decryptorIndex);
    //return readStringFromSet( _blockId, _decryptorIndex );
}


// return not-null if _daProof completes set, null otherwise (both if not enough and too much)
ptr<DecryptedAESKeyList> TEDecryptionDB::addDecryptionShares(const ::std::shared_ptr<AESKeyDecryptionShareList> &_decryptionShareList) {
    CHECK_ARGUMENT( _decryptionShareList )

    LOG( trace, "Adding daProof" );

    auto _decryptionShareListSet = this->writeStringToSet( _decryptionShareList->serializeToString(),
        _decryptionShareList->getBlockId(), _decryptionShareList->getDecryptorIndex());

    if ( _decryptionShareListSet  == nullptr ) {
        return nullptr;
    }

    CHECK_STATE( _decryptionShareListSet->size() == requiredSigners )

    auto proposalVector = getSchain()->getBiteManager()->mergeDecryptionSharesSetFromDB(_decryptionShareListSet);
    LOG( trace, "Created proposal vector" );

    return proposalVector;
}


bool TEDecryptionDB::isEnoughDecrypttions( block_id _blockID ) {
    return isEnough( _blockID );
}

#endif