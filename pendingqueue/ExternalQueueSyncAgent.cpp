/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file ExternalQueueSyncAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "ExternalQueueSyncAgent.h"
#include "../Log.h"
#include "../SkaleConfig.h"
#include "../chains/Schain.h"
#include "../datastructures/PendingTransaction.h"
#include "../datastructures/Transaction.h"
#include "../exceptions/ExitRequestedException.h"
#include "../exceptions/FatalError.h"
#include "../node/ConsensusEngine.h"
#include "../node/Node.h"
#include "../thirdparty/json.hpp"
#include "ExternalQueueSyncThread.h"
#include "PendingTransactionsAgent.h"


ExternalQueueSyncAgent::ExternalQueueSyncAgent( Schain& _sChain, ConsensusExtFace* _extFace )
    : Agent( _sChain, false ) {
    ASSERT( _sChain.getNodeCount() > 0 );

    extFace = _extFace;

    externalQueueSyncThread = make_shared< ExternalQueueSyncThread >( this );
    subChain = &_sChain;
    externalQueueSyncThread->startService();
    LOG( info, "Queue sync agent created" );
}

Schain* ExternalQueueSyncAgent::getSubChain() {
    return ( Schain* ) subChain;
}


void ExternalQueueSyncAgent::workerThreadMessagePushLoop( ExternalQueueSyncAgent* agent ) {
    setThreadName( __CLASS_NAME__ );

    agent->waitOnGlobalStartBarrier();

    try {
        while ( true ) {
            ConsensusExtFace::transactions_vector transactions;

            do {
                agent->getSubChain()->getNode()->exitCheck();
                transactions = agent->extFace->pendingTransactions(
                    agent->getNode()->getMaxTransactionsPerBlock() );
            } while ( transactions.size() == 0 );

            LOG( debug,
                "ExternalQueueSyncAgent got transaqctions " + to_string( transactions.size() ) );

            auto txs = make_shared< vector< ptr< Transaction > > >();

            for ( const auto& t : transactions ) {
                auto data = make_shared< vector< uint8_t > >( t );
                auto transaction = make_shared< PendingTransaction >( data );
                txs->push_back( transaction );
            }

            agent->getSubChain()->getPendingTransactionsAgent()->pushTransactions( txs );
        };
    } catch ( ExitRequestedException& ) {
        return;
    }
}
