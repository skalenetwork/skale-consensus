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


using namespace std;


TEDecryptionDB::TEDecryptionDB(
    Schain* _sChain, string& _dirName, string& _prefix, node_id _nodeId, uint64_t _maxDBSize )
    : CacheLevelDB( _sChain, _dirName, _prefix, _nodeId, _maxDBSize,
          LevelDBOptions::getTEDecryptionDBOptions(), false ) {
    threadPoolExecutor = std::make_shared<folly::CPUThreadPoolExecutor>(NUM_BITE_VALIDATION_THREADS);
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
        decryptionsVec, getSchain()->getCryptoManager(), false );
}



void TEDecryptionDB::addDecryptionShares(
    const ::std::shared_ptr< AESKeyDecryptionShareList >& _decryptionShareList ) {
    CHECK_ARGUMENT( _decryptionShareList );

    // we dont need to store shares twice if we have shares from this node already
    auto index = _decryptionShareList->getDecryptorIndex();
    if ( decryptionsStore.count(_decryptionShareList->getBlockId()) > 0 )
        if ( decryptionsStore.at(_decryptionShareList->getBlockId()).count(index) > 0 )
            return;

    auto serializedList = BiteAESDecryptionShareSerializer::serialize( _decryptionShareList );
    CHECK_STATE( serializedList );

    std::vector<folly::Future<folly::Unit>> futures;
    futures.reserve(decryptionShareSets.size());
    auto blockId = _decryptionShareList->getBlockId();

    {
        WRITE_LOCK(decryptionSetsMutex);

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

        decryptionShareListSet[index] = _decryptionShareList;

        if (decryptionShareSets[blockId].empty()) {
            for ( auto&& decryptionShareIterator : _decryptionShareList->getDecryptionShares() ) {
                decryptionShareSets[blockId][decryptionShareIterator.first] =
                    sChain->getBiteManager()->createAESDecryptionShareSet(
                            blockId, decryptionShareIterator.first );
            }
        }
    }

    for (const auto& share: _decryptionShareList->getDecryptionShares()) {
        auto future = folly::via(threadPoolExecutor.get(), [blockId, this, share]() -> folly::Unit {
            decryptionShareSets[blockId][share.first]->addDecryptionShare( share.second );
            return folly::unit;
        });
        futures.push_back(std::move(future));
    }
    auto allResults = folly::collectAll(futures).get();
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

ptr< DecryptedAESKeyList > TEDecryptionDB::mergeAESKeys(block_id _blockId, ptr<EncryptedAESKeyMap> _EncryptedAESKeyMap) {
    CHECK_STATE(_EncryptedAESKeyMap);

    WRITE_LOCK(decryptionSetsMutex);

    map< schain_index, ptr< AESKeyDecryptionShareList > >& decryptionShareMap =
        decryptionsStore[_blockId];

    CHECK_STATE( decryptionShareMap.size() >= requiredSigners )

    auto firstDecryptionShareList = decryptionShareMap.begin()->second;
    // TODO - count of decryption shares need to be in DA header
    CHECK_STATE( firstDecryptionShareList );
    auto size = firstDecryptionShareList->getSize();

    for (auto&& it : decryptionShareMap) {
        auto list = it.second;
        CHECK_STATE(list->getSize() == size);
    }

    map<transaction_index, ptr<AESKeyDecryptionShareSet>> decryptionShareSets;
    map<transaction_index, libBLS::CipheredKey> encryptions;

    for ( auto&& decryptionShareIterator : firstDecryptionShareList->getDecryptionShares() ) {
        decryptionShareSets[decryptionShareIterator.first] =
            sChain->getBiteManager()->createAESDecryptionShareSet(
                _blockId, decryptionShareIterator.first );
        if (sChain->getNode()->isSgxEnabled()) {
            // fill the map to use in multiple threads later if real signatures are enabled
            // dont validate inputs - were already validated before
            bool toValidate = false;
            encryptions[decryptionShareIterator.first] =
                    libBLS::CipheredKey::fromBytes(*_EncryptedAESKeyMap->at(decryptionShareIterator.first)->getKey(),
                                                   toValidate);
        }
    }

    // prepare TE public key shares if real signatures are enabled
    vector<libBLS::TEPublicKeyShare> tePublicKeys;
    if (sChain->getNode()->isSgxEnabled()) {
        ptr<vector<ptr<libBLS::BLSPublicKeyShare>>> keyShares =
                std::make_shared<vector<ptr<libBLS::BLSPublicKeyShare>>>(sChain->getCryptoManager()->getAllBlsPublicKeyShares());
        for (size_t i = 0; i < totalSigners; ++i) {
            tePublicKeys.push_back(libBLS::TEPublicKeyShare(keyShares->at(i)->getPublicKey(),
                                                       i + 1, requiredSigners, totalSigners) );
        }
    }

    std::vector<folly::Future<folly::Unit>> futures;
    futures.reserve(decryptionShareSets.size());

    auto aesKeys = make_shared< DecryptedAESKeyList >();
    std::mutex aesKeysMutex;

    for ( auto&& decryptionSharesSetIterator: decryptionShareSets ) {
        auto transactionIndex = decryptionSharesSetIterator.first;
        auto future = folly::via(threadPoolExecutor.get(), [&decryptionShareMap,
                                 &decryptionShareSets, &aesKeys, &aesKeysMutex, &tePublicKeys,
                                 &_EncryptedAESKeyMap, transactionIndex, &encryptions,
                                 sChain = this->sChain]() -> folly::Unit {
            auto decryptionSharesSet = decryptionShareSets[transactionIndex];

            // still not enough shares - validate & add more
            if ( !decryptionSharesSet->isEnough() ) {
                // data to send to the batch validation
                std::vector< libBLS::CipheredKey > cipheredKeys;
                // shares at libBLS level
                std::vector< libBLS::TEDecryptionShare > shares;
                std::vector< libBLS::TEPublicKeyShare > publicKeys;
                shares.reserve(decryptionShareMap.size());
                publicKeys.reserve(decryptionShareMap.size());

                // additional data to track decryptor indices
                std::vector< schain_index > decryptorIndices;
                // shares at consensus level
                std::vector< ptr< AESKeyDecryptionShare > > sharesList;
                sharesList.reserve(decryptionShareMap.size());

                // collect all shares for this transaction index
                for ( auto&& [_, decryptionSharesList]: decryptionShareMap) {
                    try {

                        // add consensus level share
                        auto share = decryptionSharesList->getDecryptionShare(transactionIndex);
                        CHECK_STATE(share);
                        sharesList.push_back(share);
                        size_t decryptorIndex = (size_t)share->getDecryptorIndex();

                        // Only add share for validation if real signatures are enabled
                        if (sChain->getNode()->isSgxEnabled()) {
                            // this conversion only works when using real validation. Else, it will be of Mockup type
                            auto shareConsensus = dynamic_cast<ConsensusAESKeyDecryptionShare*>(share.get());
                            CHECK_STATE(shareConsensus);

                            auto shareTE = shareConsensus->getTEDecryptionShare();
                            shares.push_back(*shareTE);
                            publicKeys.push_back(tePublicKeys.at(decryptorIndex - 1));
                        }
                        
                    }  catch ( const std::exception& ex ) {
                        CONS_LOG(err, std::string("Error during adding shares: ") + ex.what());
                    }
                }

                // verify shares batch if real signatures are enabled
                if (sChain->getNode()->isSgxEnabled() ) {
                    // push the 1 ciphertext (our batch is only for 1 ciphertext)
                    cipheredKeys.push_back(encryptions.at(transactionIndex));
                    auto result = libBLS::ThresholdEncryption::validateDecryptionSharesBatch(
                            cipheredKeys, shares, publicKeys);

                    for (size_t i = 0; i < result.size(); ++i) {
                        if (result[i]) {
                            // Only add valid shares
                            decryptionSharesSet->addDecryptionShare(sharesList[i]);
                        }
                        else {
                            CONS_LOG(err, "Decryption share validation failed for transaction: " + 
                                std::to_string(static_cast<uint32_t>(transactionIndex)));
                        }
                    }
                }
                else {
                    // no real signatures - just add all shares
                    for ( auto&& share: sharesList ) {
                        decryptionSharesSet->addDecryptionShare( share );
                    }
                }
            }

            // enough shares - merge
            if ( decryptionSharesSet->isEnough() ) {
                auto key = decryptionSharesSet->verifyAndMergeAESKey(_EncryptedAESKeyMap->at(transactionIndex));
                CHECK_STATE( key );
                std::lock_guard<std::mutex> lock(aesKeysMutex);
                aesKeys->addKey( transactionIndex, *key );
            }

            return folly::unit;
        });
        futures.push_back(std::move(future));
    }
    auto allResults = folly::collectAll(futures).get();
    // clean old shares if they exist in the map
    // we keep in the map shares for the current block and for the previous block
    // Find first element >= previous block
    auto it = decryptionsStore.lower_bound(_blockId - 1);
    // remove all old shares with block ids < previous block
    if (it != decryptionsStore.end()) {
        decryptionsStore.erase(decryptionsStore.begin(), it);
    }

    CHECK_STATE(decryptionsStore.size() <= 2 * totalSigners);
    CHECK_STATE2(aesKeys->getSize() == _EncryptedAESKeyMap->size(), "Not all aes keys could be decrypted");
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
        data, getSchain()->getCryptoManager(), false );

    addDecryptionShares(shares);

    return shares;
}


bool TEDecryptionDB::isEnoughDecryptions( block_id _blockID ) {
    READ_LOCK(decryptionSetsMutex);
    const auto it = decryptionsStore.find(_blockID);
    if ( it == decryptionsStore.end() )
        return false;
    return it->second.size() == requiredSigners;
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
    bool hasOwnShare = shares.find(sChain->getSchainIndex()) != shares.end();

    // if the DB already has the own share, it needs to contain required shares
    // Else, it needs to contain required - 1 shares
    size_t requiredShares = hasOwnShare ? requiredSigners : requiredSigners - 1;
    return shares.size() >= requiredShares;
}


uint64_t TEDecryptionDB::getDecryptionsCount( block_id _blockID ) {
    READ_LOCK(decryptionSetsMutex);
    const auto it = decryptionsStore.find(_blockID);
    if ( it != decryptionsStore.end() )
        return it->second.size();
    return 0;
};

#endif
