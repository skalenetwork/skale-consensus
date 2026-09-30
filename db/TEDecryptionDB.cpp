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

#include <atomic>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/futures/Future.h>
#include <folly/Unit.h>

#include "TEDecryptionDB.h"

#include "SkaleCommon.h"
#include "Log.h"
#include "thirdparty/json.hpp"
#include "chains/Schain.h"
#include "datastructures/BlockProposal.h"
#include "crypto/CryptoManager.h"
#include "crypto/DecryptedAESKeyList.h"
#include "crypto/AESKeyDecryptionShare.h"
#include "crypto/AESKeyDecryptionShareList.h"


#include "leveldb/db.h"
#include <crypto/EncryptedAESKey.h>
#include "crypto/ThresholdSigShare.h"
#include "LevelDBOptions.h"


#include <bite/BiteManager.h>
#include <bite/BiteAESDecryptionShareSerializer.h>
#include <crypto/AESKeyDecryptionShareSet.h>
#include <crypto/ConsensusAESKeyDecryptionShare.h>

#include <bls/BLSPublicKeyShare.h>

#include "datastructures/TransactionCiphertextsMap.h"


using namespace std;


TEDecryptionDB::TEDecryptionDB(
    Schain* _sChain, string& _dirName, string& _prefix, node_id _nodeId, uint64_t _maxDBSize )
    : CacheLevelDB( _sChain, _dirName, _prefix, _nodeId, _maxDBSize,
          LevelDBOptions::getTEDecryptionDBOptions(), false ) {

    threadPoolExecutor = std::make_shared<folly::CPUThreadPoolExecutor>(getNumBiteValidationThreads());
}

TEDecryptionDB::~TEDecryptionDB() {
    if ( threadPoolExecutor ) {
        threadPoolExecutor->stop();
        threadPoolExecutor->join();
    }
}

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
        decryptionsVec, getSchain()->getCryptoManager(), 
        CryptographicValidationMode::SkipValidationTrustedSource );
}



void TEDecryptionDB::addDecryptionShares(
    const ::std::shared_ptr< AESKeyDecryptionShareList >& _decryptionShareList ) {
    CHECK_ARGUMENT( _decryptionShareList );

    auto index = _decryptionShareList->getDecryptorIndex();
    auto blockId = _decryptionShareList->getBlockId();

    {
        WRITE_LOCK(decryptionSetsMutex);

        // decryptor - all shares for all txs
        map< schain_index, ptr< AESKeyDecryptionShareList > >& decryptionShareListSet =
            decryptionsStore[blockId];

        // One slot per decryptor, first list wins. The shape is not checked here: the reference
        // is the proposal, which may not be known yet. mergeAESKeys() checks it.
        if (decryptionShareListSet.count(index) > 0 ||
            decryptionShareListSet.size() >= totalSigners) {
            return;
        }

        if (!decryptionShareListSet.empty()) {
            auto firstShare = decryptionShareListSet.begin()->second;
            CHECK_STATE( firstShare->getProposerIndex() == _decryptionShareList->getProposerIndex() );
        }

        decryptionShareListSet[index] = _decryptionShareList;
    }

    // Publish progress for both peer responses and asynchronously computed local shares.
    decryptionSharesChanged.notify_all();
};

bool TEDecryptionDB::haveDecryptionShares(block_id _blockID, schain_index _decryptorIndex) {
    CHECK_ARGUMENT(_decryptorIndex > 0);
    CHECK_ARGUMENT(_decryptorIndex <= totalSigners);

    READ_LOCK(decryptionSetsMutex);

    const auto it = decryptionsStore.find(_blockID);
    if ( it == decryptionsStore.end() )
        return false;

    return it->second.count(_decryptorIndex) > 0;

};

bool TEDecryptionDB::waitForMoreDecryptionShares(block_id _blockId, uint64_t _previousCount,
    std::chrono::milliseconds _timeout) {
    std::unique_lock<shared_mutex> lock(decryptionSetsMutex);
    return decryptionSharesChanged.wait_for(lock, _timeout, [&] {
        const auto it = decryptionsStore.find(_blockId);
        return it != decryptionsStore.end() && it->second.size() > _previousCount;
    });
}

ptr< DecryptedAESKeyList > TEDecryptionDB::mergeAESKeys(block_id _blockId,
    ptr<TransactionCiphertextsMap> _transactionCiphertextsMap, uint64_t* _sharesUsed) {
    CHECK_STATE(_transactionCiphertextsMap);

    // Merge a stable snapshot while workers continue storing additional share lists.
    map<schain_index, ptr<AESKeyDecryptionShareList>> decryptionShareMap;
    {
        READ_LOCK(decryptionSetsMutex);
        const auto it = decryptionsStore.find(_blockId);
        if (it != decryptionsStore.end()) {
            decryptionShareMap = it->second;
        }
        if (_sharesUsed) {
            *_sharesUsed = decryptionShareMap.size();
        }
    }

    // 1 key for each signer (index = signerId)
    vector<libBLS::TEPublicKeyShare> tePublicKeys;
    if (sChain->getNode()->isSgxEnabled()) {
        ptr<vector<ptr<libBLS::BLSPublicKeyShare>>> keyShares =
                std::make_shared<vector<ptr<libBLS::BLSPublicKeyShare>>>(
                    sChain->getCryptoManager()->getAllBlsPublicKeyShares());

        CHECK_STATE(keyShares->size() == totalSigners);
        
        for (size_t i = 0; i < totalSigners; ++i) {
            tePublicKeys.push_back(libBLS::TEPublicKeyShare(keyShares->at(i)->getPublicKey(),
                                                       i + 1, requiredSigners, totalSigners) );
        }
    }

    auto aesKeys = sChain->getBiteManager()->mergeAESKeys(
        _blockId,
        *_transactionCiphertextsMap,
        decryptionShareMap,
        tePublicKeys,
        BiteRuntimeContext{ sChain->getNode()->isSgxEnabled(), threadPoolExecutor }
    );

    WRITE_LOCK(decryptionSetsMutex);

    // clean old shares if they exist in the map
    // we keep in the map shares for the current block and for the previous block
    // Find first element >= previous block
    auto it = decryptionsStore.lower_bound(_blockId - 1);
    // remove all old shares with block ids < previous block
    if (it != decryptionsStore.end()) {
        decryptionsStore.erase(decryptionsStore.begin(), it);
    }

    CHECK_STATE(decryptionsStore.size() <= 2 * totalSigners);

    return aesKeys;
}


void TEDecryptionDB::addMyDecryptionShares(
    const ::std::shared_ptr< AESKeyDecryptionShareList >& _decryptionShareList ) {
    CHECK_ARGUMENT( _decryptionShareList )

    addDecryptionShares(_decryptionShareList);

    auto serializedList = BiteAESDecryptionShareSerializer::serialize( _decryptionShareList );
    CHECK_STATE( serializedList );

    auto key = createKey( ( _decryptionShareList->getBlockId() ),
                   _decryptionShareList->getProposerIndex() ) +
               ".my";

    writeByteArray( key, serializedList );
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

    auto shares =  BiteAESDecryptionShareSerializer::deserialize(
        data, getSchain()->getCryptoManager(),
        CryptographicValidationMode::SkipValidationTrustedSource );

    addDecryptionShares(shares);

    return shares;
}


bool TEDecryptionDB::isEnoughDecryptions( block_id _blockID ) {
    READ_LOCK(decryptionSetsMutex);
    const auto it = decryptionsStore.find(_blockID);
    if ( it == decryptionsStore.end() )
        return false;
    return it->second.size() >= static_cast<size_t>(requiredSigners);
};

bool TEDecryptionDB::isEnoughForeignShares(block_id _blockID) {
    // if required signers < 2, we always have our own share
    if ( requiredSigners < 2 )
        return true;

    READ_LOCK(decryptionSetsMutex);

    const auto it = decryptionsStore.find(_blockID);
    if ( it == decryptionsStore.end() )
        return false;

    const auto& shares = it->second;
    // A full threshold is always required.
    // - own share present : shares.size() includes own + (requiredSigners-1) foreign
    // - own share absent  : all requiredSigners must be foreign so merge can succeed
    return shares.size() >= static_cast<size_t>(requiredSigners);
}


uint64_t TEDecryptionDB::getDecryptionsCount( block_id _blockID ) {
    READ_LOCK(decryptionSetsMutex);
    const auto it = decryptionsStore.find(_blockID);
    if ( it != decryptionsStore.end() )
        return it->second.size();
    return 0;
};

#endif
