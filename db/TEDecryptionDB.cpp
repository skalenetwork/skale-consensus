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

        // decryptor - all shares for all txs
        map< schain_index, ptr< AESKeyDecryptionShareList > >& decryptionShareListSet =
            decryptionsStore[_decryptionShareList->getBlockId()];

        if ( decryptionShareListSet.size() >= requiredSigners ) {
            return;
        }

        if (!decryptionShareListSet.empty()) {
            auto firstShare = decryptionShareListSet.begin()->second;
            CHECK_STATE( firstShare->getProposerIndex() == _decryptionShareList->getProposerIndex() );
            // all decryptors should send shares for same number of ciphertexts
            CHECK_STATE( firstShare->totalCiphertextSharesCount() == _decryptionShareList->totalCiphertextSharesCount() );
        }

        decryptionShareListSet[index] = _decryptionShareList;

        if (decryptionShareSets[blockId].empty()) {
            for ( auto&& [txId, shares] : _decryptionShareList->getDecryptionShares() ) {
                decryptionShareSets[blockId][txId] =
                    sChain->getBiteManager()->createAESDecryptionShareSet(
                            blockId, txId, shares->size() );
            }
        }
    }

    for (const auto& [txIdx, shares]: _decryptionShareList->getDecryptionShares()) {
        auto future = folly::via(threadPoolExecutor.get(), [blockId, this, shares, txIdx]() -> folly::Unit {
            decryptionShareSets[blockId][txIdx]->addDecryptionSharesFromSameDecryptor( shares );
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

ptr< DecryptedAESKeyList > TEDecryptionDB::mergeAESKeys(block_id _blockId, ptr<TransactionCiphertextsMap> _transactionCiphertextsMap) {
    CHECK_STATE(_transactionCiphertextsMap);

    WRITE_LOCK(decryptionSetsMutex);

    // nodeId -> decryption shares (each may hold decryption shares for multiple ciphertexts)
    map< schain_index, ptr< AESKeyDecryptionShareList > >& decryptionShareMap =
        decryptionsStore[_blockId];

    CHECK_STATE( decryptionShareMap.size() >= requiredSigners )

    auto firstDecryptionShareList = decryptionShareMap.begin()->second;
    // TODO - count of decryption shares need to be in DA header
    CHECK_STATE( firstDecryptionShareList );
    auto size = firstDecryptionShareList->totalCiphertextSharesCount();

    for (auto&& [_, list] : decryptionShareMap) {
        CHECK_STATE(list->totalCiphertextSharesCount() == size);
    }

    // 1 per transaction (but each tx may contain multiple ciphertexts)
    map<transaction_index, ptr<AESKeyDecryptionShareSet>> decryptionShareSets;
    map<transaction_index, std::vector<libBLS::CipheredKey>> encryptions;

    bool toValidate = false;
    for ( auto&& [idx, aesDecryptionShare] : firstDecryptionShareList->getDecryptionShares() ) {
        decryptionShareSets[idx] =
            sChain->getBiteManager()->createAESDecryptionShareSet(
                _blockId, idx, aesDecryptionShare->size() );
        if (sChain->getNode()->isSgxEnabled()) {
            // fill the map to use in multiple threads later if real signatures are enabled
            // dont validate inputs - were already validated before
            auto& ciphertextsForCurrTx = *_transactionCiphertextsMap->at(idx);
            for (auto& ciphertext : ciphertextsForCurrTx) {
                encryptions[idx].push_back(
                    libBLS::CipheredKey::fromBytes(ciphertext.data(), toValidate));
            }
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

    std::vector<folly::Future<folly::Unit>> futures;
    futures.reserve(decryptionShareSets.size());

    auto aesKeys = make_shared< DecryptedAESKeyList >();
    std::mutex aesKeysMutex;

    for ( auto&& [txId, decryptionSet]: decryptionShareSets ) {
        auto future = folly::via(threadPoolExecutor.get(), [&decryptionShareMap,
                                 &decryptionShareSets, decryptionSet, totalSigners = this->totalSigners,
                                 &aesKeys, &aesKeysMutex, &tePublicKeys,
                                 &_transactionCiphertextsMap, txId, &encryptions,
                                 sChain = this->sChain]() -> folly::Unit {

            size_t numberOfCiphertexts = _transactionCiphertextsMap->at(txId)->count();
            // still not enough shares - validate & add more
            if ( !decryptionSet->isEnough() ) {

                // shares at libBLS level (ciphertext idx -> list of shares for that ciphertext)
                std::vector< std::vector< libBLS::TEDecryptionShare > > teShares;
                std::vector< std::vector< libBLS::TEPublicKeyShare > > publicKeys;
                // additional data to track decryptor indices
                std::vector< size_t > decryptorIndices;
                // shares at consensus level
                // each index holds a list of shares for all ciphertexts within current tx for some decryptor
                std::vector< ptr< AESKeyDecryptionShares > > sharesList;
                // initialize vectors
                teShares.assign(numberOfCiphertexts, {});
                publicKeys.assign(numberOfCiphertexts, {});
                sharesList.resize(totalSigners);
                for (size_t i = 0; i < totalSigners; ++i) {
                    sharesList[i] = make_shared<AESKeyDecryptionShares>();
                }

                // collect all shares from all nodes for current Tx
                for ( auto&& [decryptorIdx, decryptionSharesList]: decryptionShareMap) {
                    try {
                        // shares for all ciphertexts within current tx from current decryptor
                        ptr<AESKeyDecryptionShares> ciphertextsShares = decryptionSharesList->getDecryptionShares(txId);
                        CHECK_STATE(ciphertextsShares);
                        // every decryptor should provide shares for all ciphertexts
                        CHECK_STATE(ciphertextsShares->size() == numberOfCiphertexts);

                        size_t decryptorIndex = (size_t)decryptorIdx - 1;
                        decryptorIndices.push_back(decryptorIndex);
                        sharesList.at(decryptorIndex) = ciphertextsShares;
                
                        // Only add share for validation if real signatures are enabled
                        if (sChain->getNode()->isSgxEnabled()) {

                            for (size_t i = 0; i < numberOfCiphertexts; ++i) {

                                // this conversion only works when using real validation. Else, it will be of Mockup type
                                auto shareConsensus = std::dynamic_pointer_cast<ConsensusAESKeyDecryptionShare>(ciphertextsShares->at(i));
                                CHECK_STATE(shareConsensus);

                                auto shareTE = shareConsensus->getTEDecryptionShare();
                                teShares.at(i).push_back(*shareTE);
                                publicKeys.at(i).push_back(tePublicKeys.at(decryptorIndex));
                            }
                        }
                        
                    }  catch ( const std::exception& ex ) {
                        CONS_LOG(err, std::string("Error during adding shares: ") + ex.what());
                    }
                }

                // verify shares batch if real signatures are enabled
                if (sChain->getNode()->isSgxEnabled() ) {

                    std::vector<bool> allSharesFromNodeAreValid;
                    allSharesFromNodeAreValid.resize(totalSigners, true);

                    for (size_t ciphertextId = 0; ciphertextId < numberOfCiphertexts; ++ciphertextId) {
                        std::vector<libBLS::CipheredKey> cipheredKeys{ encryptions.at(txId).at(ciphertextId) };

                        auto result = libBLS::ThresholdEncryption::validateDecryptionSharesBatch(
                                cipheredKeys, teShares.at(ciphertextId), publicKeys.at(ciphertextId));

                        for (size_t shareId = 0; shareId < result.size(); ++shareId) {
                            if (!result[shareId]) {
                                // shares from this node are invalid
                                allSharesFromNodeAreValid[decryptorIndices[shareId]] = false;
                                CONS_LOG(err, fmt::format(
                                    "Decryption share validation failed: tx_id={}, ciphertext_id={}, share_id={}",
                                    (uint32_t)txId, ciphertextId, shareId)
                                );
                            }
                        }
                    }

                    // Only add shares for current transaction if all shares for all ciphertexts are valid for some node
                    for (size_t i = 0; i < decryptorIndices.size(); ++i) {
                        if (allSharesFromNodeAreValid[decryptorIndices[i]]) {
                            // add all valid shares
                            decryptionSet->addDecryptionSharesFromSameDecryptor(sharesList[decryptorIndices[i]]);
                        }
                    }
                }
                else {
                    for (size_t i = 0; i < decryptorIndices.size(); ++i) {
                        // add all valid shares
                        decryptionSet->addDecryptionSharesFromSameDecryptor(sharesList[decryptorIndices[i]]);
                    }
                }
            }

            // enough shares - merge
            if ( decryptionSet->isEnough() ) {
                auto keys = decryptionSet->verifyAndMergeAESKeys(_transactionCiphertextsMap->at(txId)->getCiphertexts());
                CHECK_STATE( keys );
                std::lock_guard<std::mutex> lock(aesKeysMutex);
                aesKeys->addKeys( txId, *keys );
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
    CHECK_STATE2(aesKeys->totalDecryptedCiphertextsCount() == _transactionCiphertextsMap->totalCiphertextCount(), 
        "Not all aes keys could be decrypted");
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
