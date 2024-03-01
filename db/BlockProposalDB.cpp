/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file ReceivedBlockProposalsDatabase.cpp
    @author Stan Kladko
    @date 2018
*/

#include "Agent.h"
#include "SkaleCommon.h"
#include "Log.h"
#include "abstracttcpserver/ConnectionStatus.h"
#include "blockproposal/pusher/BlockProposalClientAgent.h"
#include "chains/Schain.h"
#include "crypto/BLAKE3Hash.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/BlockProposalSet.h"
#include "datastructures/CommittedBlock.h"
#include "datastructures/DAProof.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/FatalError.h"
#include "leveldb/db.h"
#include "monitoring/LivelinessMonitor.h"
#include "monitoring/OptimizerAgent.h"
#include "node/Node.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "thirdparty/json.hpp"

#include "LevelDBOptions.h"
#include "BlockProposalDB.h"


using namespace std;

#define PROPOSAL_CACHE_SIZE 3

BlockProposalDB::BlockProposalDB(
    Schain* _sChain, string& _dirName, string& _prefix, node_id _nodeId, uint64_t _maxDBSize )
    : CacheLevelDB( _sChain, _dirName, _prefix, _nodeId, _maxDBSize,
          LevelDBOptions::getBlockProposalDBOptions(), true ) {
    proposalCaches = make_shared< vector< ptr< BlockProposal > > >();

    for ( int i = 0; i < _sChain->getNodeCount(); i++ ) {
        proposalCaches->push_back( nullptr );
    }
};


/* We store foreign proposals in memory and write own proposal in storage
 * this is done to be able to re-propose the same thing in case of a crash
 */

void BlockProposalDB::addBlockProposal( const ptr< BlockProposal > _proposal ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ );

    CHECK_ARGUMENT( _proposal );
    CHECK_ARGUMENT( _proposal->getSignature() != "" );
    auto proposerIndex = _proposal->getProposerIndex();
    CHECK_STATE( ( uint64_t ) proposerIndex <= getSchain()->getNodeCount() );
    CHECK_STATE( proposerIndex > 0 );

    LOG( trace, "addBlockProposal blockID_=" << to_string( _proposal->getBlockID() )
                                             << " proposerIndex="
                                             << to_string( _proposal->getProposerIndex() ) );

    addProposalToCacheIfDoesNotExist( _proposal );


    // save own proposal to levelDB
    if ( _proposal->getProposerIndex() == getSchain()->getSchainIndex() ) {

        // for optimized consensus only previous winner proposed.
        // non-winners skip sending proposal and do not need to save it to the db
        // since saving proposal to the db is done to be able to resend it in case of a
        // crash
        if (getSchain()->getOptimizerAgent()->skipSendingProposalToTheNetwork(_proposal->getBlockID()))
                return;

        serializeProposalAndSaveItToLevelDB( _proposal );
    }
}
void BlockProposalDB::addProposalToCacheIfDoesNotExist( const ptr< BlockProposal > _proposal ) {
    auto key = createKey( _proposal->getBlockID(), _proposal->getProposerIndex() );

    auto proposerIndex = ( uint64_t ) _proposal->getProposerIndex();

    CHECK_STATE( proposalCaches )
    CHECK_STATE( proposerIndex > 0 );
    CHECK_STATE( proposerIndex <= proposalCaches->size() )

    {
        WRITE_LOCK( proposalCacheMutex )
        auto previousProposal = proposalCaches->at( ( uint64_t ) proposerIndex - 1 );
        if ( previousProposal ) {
            if ( previousProposal->getBlockID() > ( uint64_t ) _proposal->getBlockID() ) {
                LOG( warn,
                    "Trying to add a proposal with smaller block id:" << _proposal->getBlockID() );
                return;
            }

            if ( previousProposal->getBlockID() > ( uint64_t ) _proposal->getBlockID() ) {
                LOG( warn,
                    "Trying to add a proposal with same block id:" << _proposal->getBlockID() );
                return;
            }
        }

        proposalCaches->at( ( uint64_t ) proposerIndex - 1 ) = _proposal;
    }
}
void BlockProposalDB::serializeProposalAndSaveItToLevelDB( const ptr< BlockProposal > _proposal ) {
    CHECK_STATE( _proposal );

    try {
        ptr< vector< uint8_t > > serialized;

        serialized = _proposal->serializeProposal();
        CHECK_STATE( serialized );

        writeByteArrayToSet( ( const char* ) serialized->data(), serialized->size(),
            _proposal->getBlockID(), _proposal->getProposerIndex() );

    } catch ( ExitRequestedException& e ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


ptr< vector< uint8_t > > BlockProposalDB::getMyProposalFromLevelDB(
    block_id _blockID, schain_index _proposerIndex ) {
    try {
        auto value = readStringFromSet( _blockID, _proposerIndex );

        if ( value != "" ) {
            auto serializedBlock = make_shared< vector< uint8_t > >();
            serializedBlock->insert(
                serializedBlock->begin(), value.data(), value.data() + value.size() );
            CommittedBlock::serializedSanityCheck( serializedBlock );
            return serializedBlock;
        } else {
            return nullptr;
        }
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


ptr< BlockProposal > BlockProposalDB::getBlockProposal(
    block_id _blockID, schain_index _proposerIndex ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    ptr< BlockProposal > p = nullptr;

    auto key = createKey( _blockID, _proposerIndex );

    CHECK_STATE( proposalCaches )

    {
        READ_LOCK( proposalCacheMutex )
        auto cachedProposal = proposalCaches->at( ( uint64_t ) _proposerIndex - 1 );

        if ( cachedProposal ) {
            if ( cachedProposal->getBlockID() == ( uint64_t ) _blockID ) {
                return cachedProposal;
            }
        }
    }

    if ( getSchain()->getSchainIndex() != _proposerIndex ) {
        // non-owned proposals are never saved in DB
        return nullptr;
    }

    auto serializedProposal = getMyProposalFromLevelDB( _blockID, _proposerIndex );

    if ( serializedProposal == nullptr )
        return nullptr;


    // dont check signatures on proposals stored in the db since they have already been verified
    auto proposal =
        BlockProposal::deserialize( serializedProposal, getSchain()->getCryptoManager(), false );

    if ( !proposal )
        return nullptr;

    CHECK_STATE( !proposal->getSignature().empty() );

    return proposal;
}


const string& BlockProposalDB::getFormatVersion() {
    static const string version = "1.0";
    return version;
}

void BlockProposalDB::cleanupUnneededMemoryBeforePushingToEvm(
    const ptr< CommittedBlock > _block ) {
    CHECK_STATE( _block )
    WRITE_LOCK( proposalCacheMutex )


    auto proposerIndex = _block->getProposerIndex();
    auto blockId = _block->getBlockID();

    // keep only the winning proposal in cache since
    // the noone will ask for losers

    for ( uint64_t i = 0; i < proposalCaches->size(); i++ ) {
        auto cachedProposal = proposalCaches->at( i );
        if ( cachedProposal ) {
            if ( ( cachedProposal->getProposerIndex() != proposerIndex ) ||
                 ( cachedProposal->getBlockID() <= blockId ) ) {
                proposalCaches->at( i ) = nullptr;
            }
        }
    }
}
