/*
    Copyright (C) 2018- SKALE Labs

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

    @file PendingTransactionsAgent.cpp
    @author Stan Kladko
    @date 2018-
*/

#include <algorithm>
#include "Log.h"
#include "SkaleCommon.h"
#include "db/BlockDB.h"
#include "db/CacheLevelDB.h"
#include "exceptions/FatalError.h"
#include "datastructures/Transaction.h"
#include "leveldb/db.h"
#include "thirdparty/json.hpp"
#include <monitoring/LivelinessMonitor.h>
#include <unordered_set>

#include "PendingTransactionsAgent.h"
#include "chains/Schain.h"
#include "crypto/CryptoManager.h"
#include "crypto/BLAKE3Hash.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/MyBlockProposal.h"
#include "datastructures/PartialHashesList.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"
#include "db/CommittedTransactionDB.h"
#include "node/ConsensusEngine.h"
#include "node/Node.h"
#include "pendingqueue/TestMessageGeneratorAgent.h"
#include "utils/Time.h"

#include "microprofile.h"

using namespace std;


PendingTransactionsAgent::PendingTransactionsAgent( Schain& ref_sChain )
    : Agent( ref_sChain, false ) {}

ptr< BlockProposal > PendingTransactionsAgent::buildBlockProposal(
    block_id _blockID, TimeStamp& _previousBlockTimeStamp, bool _isCalledAfterCatchup ) {
    MICROPROFILE_ENTERI( "PendingTransactionsAgent", "sleep", MP_DIMGRAY );
    usleep( getNode()->getMinBlockIntervalMs() * 1000 );
    MICROPROFILE_LEAVE();

    auto result = createTransactionsListForProposal( _isCalledAfterCatchup );
    transactionListReceivedTimeMs = Time::getCurrentTimeMs();
    auto transactions = result.first;
    CHECK_STATE( transactions );
    auto stateRoot = result.second;

    while ( Time::getCurrentTimeMs() <=
            _previousBlockTimeStamp.getS() * 1000 + _previousBlockTimeStamp.getMs() ) {
        usleep( 10 );
    }

    auto transactionList = make_shared< TransactionList >( transactions );

    auto stamp = TimeStamp::getCurrentTimeStamp();

    auto myBlockProposal = make_shared< MyBlockProposal >( *sChain, _blockID,
        sChain->getSchainIndex(), transactionList, stateRoot, stamp.getS(), stamp.getMs(),
        getSchain()->getCryptoManager() );

    LOG( trace, "Created proposal, transactions:" << to_string( transactions->size() ) );

    auto pHashesList = myBlockProposal->createPartialHashesList();
    CHECK_STATE( pHashesList );

    transactionCounter += ( uint64_t ) pHashesList->getTransactionCount();

    return myBlockProposal;
}

pair< ptr< vector< ptr< Transaction > > >, u256 >
PendingTransactionsAgent::createTransactionsListForProposal( bool _isCalledAfterCatchup ) {
    MONITOR2( __CLASS_NAME__, __FUNCTION__, getSchain()->getMaxExternalBlockProcessingTime() )

    auto result = make_shared< vector< ptr< Transaction > > >();

    size_t needMax = getNode()->getMaxTransactionsPerBlock();

    ConsensusExtFace::transactions_vector txVector;

    auto startTimeMs = Time::getCurrentTimeMs();

    u256 stateRoot = 0;
    static u256 stateRootSample = 1;

    uint64_t waitTimeMs = 10;

    while ( txVector.empty() ) {
        getSchain()->getNode()->exitCheck();

        if ( sChain->getExtFace() ) {
            getSchain()->getNode()->checkForExitOnBlockBoundaryAndExitIfNeeded();
            txVector = sChain->getExtFace()->pendingTransactions( needMax, stateRoot );
            // block boundary is the safest place for exit
            // exit immediately if exit has been requested
            // this will initiate immediate exit and throw ExitRequestedException
            getSchain()->getNode()->checkForExitOnBlockBoundaryAndExitIfNeeded();
        } else {
            stateRootSample++;
            stateRoot = 7;
            txVector = sChain->getTestMessageGeneratorAgent()->pendingTransactions( needMax );
        }

        auto finishTime = Time::getCurrentTimeMs();
        auto diffTime = finishTime - startTimeMs;

        uint64_t emptyBlockTimeMs;

        if ( _isCalledAfterCatchup ) {
            // chose the smaller of two intervals. This is just to be able to force
            // empty block in tests by setting only emptyBlockInterval to zero
            emptyBlockTimeMs = std::min( getNode()->getEmptyBlockIntervalAfterCatchupMs(),
                getNode()->getEmptyBlockIntervalMs() );
        } else {
            emptyBlockTimeMs = getNode()->getEmptyBlockIntervalMs();
        }

        if ( this->sChain->getLastCommittedBlockID() == 0 || diffTime >= emptyBlockTimeMs ) {
            break;
        }

        usleep( waitTimeMs * 1000 );

        if ( waitTimeMs < 10 * 32 ) {
            waitTimeMs *= 2;
        }

    }  // while

    auto finishTimeMs = Time::getCurrentTimeMs();

    transactionListWaitTime = finishTimeMs - startTimeMs;

    for ( const auto& e : txVector ) {
        ptr< Transaction > pt = Transaction::deserialize(
            make_shared< std::vector< uint8_t > >( e ), 0, e.size(), false );
        result->push_back( pt );
        pushKnownTransaction( pt );
    }

    return { result, stateRoot };
}


ptr< Transaction > PendingTransactionsAgent::getKnownTransactionByPartialHash(
    const ptr< partial_sha_hash > hash ) {
    READ_LOCK( transactionsMutex );
    if ( knownTransactions.count( hash ) )
        return knownTransactions.at( hash );
    return nullptr;
}

void PendingTransactionsAgent::pushKnownTransaction( const ptr< Transaction >& _transaction ) {
    CHECK_ARGUMENT( _transaction );

    WRITE_LOCK( transactionsMutex );

    if ( knownTransactions.count( _transaction->getPartialHash() ) ) {
        LOG( trace, "Duplicate transaction pushed to known transactions" );
        return;
    }

    auto partialHash = _transaction->getPartialHash();

    CHECK_STATE( partialHash );

    knownTransactions[partialHash] = _transaction;
    knownTransactionsQueue.push( _transaction );
    CHECK_STATE( knownTransactions.size() == knownTransactionsQueue.size() );
    knownTransactionsTotalSize += ( _transaction->getData()->size() + PARTIAL_HASH_LEN );

    while ( knownTransactions.size() > KNOWN_TRANSACTIONS_HISTORY ||
            knownTransactionsTotalSize > MAX_KNOWN_TRANSACTIONS_TOTAL_SIZE ) {
        auto tx = knownTransactionsQueue.front();
        CHECK_STATE( tx );
        knownTransactionsTotalSize -= ( tx->getData()->size() + PARTIAL_HASH_LEN );
        CHECK_STATE( knownTransactions.count( tx->getPartialHash() ) > 0 );
        knownTransactions.erase( tx->getPartialHash() );
        knownTransactionsQueue.pop();
    }
}


uint64_t PendingTransactionsAgent::getKnownTransactionsSize() {
    READ_LOCK( transactionsMutex );
    CHECK_STATE( knownTransactions.size() == knownTransactionsQueue.size() );
    return knownTransactions.size();
}
