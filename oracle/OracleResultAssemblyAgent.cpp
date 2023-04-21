//
// Created by skale on 06.04.22.
//

#include "SkaleCommon.h"
#include "Log.h"
#include "chains/Schain.h"
#include "monitoring/LivelinessMonitor.h"
#include "OracleMessageThreadPool.h"
#include "OracleServerAgent.h"
#include "OracleResultAssemblyAgent.h"

OracleResultAssemblyAgent::OracleResultAssemblyAgent( Schain& _sChain )
    : Agent( _sChain, true ), oracleMessageThreadPool( new OracleMessageThreadPool( this ) ) {
    try {
        logThreadLocal_ = _sChain.getNode()->getLog();
        oracleMessageThreadPool->startService();
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void OracleResultAssemblyAgent::messageThreadProcessingLoop( OracleResultAssemblyAgent* _agent ) {
    CHECK_ARGUMENT( _agent );

    setThreadName( "orclAssemblyLoop", _agent->getSchain()->getNode()->getConsensusEngine() );

    _agent->getSchain()->waitOnGlobalStartBarrier();
    logThreadLocal_ = _agent->getSchain()->getNode()->getLog();

    queue< ptr< MessageEnvelope > > newQueue;

    while ( !_agent->getSchain()->getNode()->isExitRequested() ) {
        {
            unique_lock< mutex > mlock( _agent->messageMutex );
            while ( _agent->messageQueue.empty() ) {
                _agent->messageCond.wait( mlock );
                if ( _agent->getNode()->isExitRequested() )
                    return;
            }

            newQueue = _agent->messageQueue;

            while ( !_agent->messageQueue.empty() ) {
                if ( _agent->getNode()->isExitRequested() )
                    return;

                _agent->messageQueue.pop();
            }
        }

        while ( !newQueue.empty() ) {
            if ( _agent->getNode()->isExitRequested() )
                return;

            ptr< MessageEnvelope > m = newQueue.front();
            CHECK_STATE( ( uint64_t ) m->getMessage()->getBlockId() != 0 );

            try {
                if ( m->getMessage()->getMsgType() == MSG_ORACLE_REQ_BROADCAST ||
                     m->getMessage()->getMsgType() == MSG_ORACLE_RSP ) {
                    _agent->getSchain()->getOracleInstance()->routeAndProcessMessage( m );
                } else {
                    CHECK_STATE( false );
                }
            } catch ( exception& e ) {
                LOG( err, "Exception in Schain::oracleAssemblylLoop" );
                SkaleException::logNested( e );
                if ( _agent->getNode()->isExitRequested() )
                    return;
            }  // catch

            newQueue.pop();
        }
    }
}

void OracleResultAssemblyAgent::postMessage( const ptr< MessageEnvelope >& _me ) {
    CHECK_ARGUMENT( _me );

    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    getSchain()->checkForExit();

    CHECK_STATE( ( uint64_t ) _me->getMessage()->getBlockId() != 0 );
    {
        lock_guard< mutex > l( messageMutex );
        messageQueue.push( _me );
        messageCond.notify_all();
    }
}