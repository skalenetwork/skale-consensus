/*
    Copyright (C) 2021- SKALE Labs

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

    @file OracleRequestBroadcastMessage.h
    @author Stan Kladko
    @date 2021-
*/


#include "Log.h"

#include "OracleClient.h"
#include "OracleErrors.h"
#include "OracleReceivedResults.h"
#include "OracleRequestBroadcastMessage.h"
#include "OracleRequestSpec.h"
#include "OracleResponseMessage.h"
#include "OracleResult.h"
#include "SkaleCommon.h"
#include "chains/Schain.h"
#include "messages/MessageEnvelope.h"
#include "network/Network.h"
#include "node/Node.h"
#include "protocols/ProtocolInstance.h"
#include "utils/Time.h"
#include "exceptions/OracleException.h"


OracleClient::OracleClient( Schain& _sChain )
    : ProtocolInstance( ORACLE, _sChain ),
      sChain( &_sChain ),
      receiptsMap( ORACLE_RECEIPTS_MAP_SIZE ) {
    gethURL = getSchain()->getNode()->getGethUrl();

    if ( gethURL.empty() ) {
        LOG( err,
            "Consensus initialized with empty gethURL. Geth-related Oracle functions will be "
            "disabled" );
    }
}

uint64_t OracleClient::broadcastRequest( ptr< OracleRequestBroadcastMessage > _msg ) {
    try {
        CHECK_STATE( _msg )
        CHECK_STATE( sChain )


        auto receipt = _msg->getParsedSpec()->getReceipt();


        auto results = make_shared< OracleReceivedResults >( _msg->getParsedSpec(),
            getSchain()->getRequiredSigners(), ( uint64_t ) getSchain()->getNodeCount(),
            getSchain()->getNode()->isSgxEnabled() );

        auto exists = receiptsMap.putIfDoesNotExist( receipt, results );

        if ( !exists ) {
            LOG( err, "Request exists:" + receipt );
            return ORACLE_DUPLICATE_REQUEST;
        }

        LOCK( m );

        sChain->getNode()->getNetwork()->broadcastOracleRequestMessage( _msg );

        return ORACLE_SUCCESS;

    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

void OracleClient::sendTestRequestAndWaitForResult( ptr< OracleRequestSpec > _spec ) {
    CHECK_STATE( _spec );

    try {
        string _receipt;


        auto status = submitOracleRequest( _spec->getSpec(), _receipt );

        CHECK_STATE( status.first == ORACLE_SUCCESS );


        thread t( [this, _receipt]() {
            while ( true ) {
                string result;
                string r = _receipt;
                sleep( 1 );
                auto st = checkOracleResult( r, result );
                if ( st == ORACLE_SUCCESS ) {
                    cerr << result << endl;
                    return;
                }

                if ( st != ORACLE_RESULT_NOT_READY ) {
                    return;
                }
            }
        } );
        t.detach();

    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
};


pair<uint64_t,string> OracleClient::submitOracleRequest( const string& _spec, string& _receipt ) {
    ptr< OracleRequestBroadcastMessage > msg = nullptr;

    try {
        msg =
            make_shared< OracleRequestBroadcastMessage >( _spec, sChain->getLastCommittedBlockID(),
                Time::getCurrentTimeMs(), *sChain->getOracleClient() );

        auto spec = msg->getParsedSpec();

        if ( !spec ) {
            auto message = "Null spec in submitOracleRequest";
            LOG( err, message );
            return { ORACLE_INTERNAL_SERVER_ERROR, message};
        }

        if ( msg->getParsedSpec()->getChainId() != this->getSchain()->getSchainID() ) {
            auto message = string( "Invalid schain id in oracle spec:" +
                                   to_string( msg->getParsedSpec()->getChainId() ) );
            LOG( err, message );
            return { ORACLE_INVALID_CHAIN_ID, message};
        }

        if ( spec->getTime() + ORACLE_REQUEST_AGE_ON_RECEIPT_MS < Time::getCurrentTimeMs() ) {
            auto message = string( "Received old request with age:" ) +
                           to_string( Time::getCurrentTimeMs() - spec->getTime() );
            LOG( err, message );
            return { ORACLE_TIME_IN_REQUEST_SPEC_TOO_OLD, message};
        }

        if ( spec->getTime() > Time::getCurrentTimeMs() + ORACLE_REQUEST_FUTURE_JITTER_MS ) {
            auto message = string( "Received oracle request with time in the future age:" ) +
            to_string( spec->getTime() - Time::getCurrentTimeMs() );
            LOG( err, message );
            return {ORACLE_TIME_IN_REQUEST_SPEC_IN_THE_FUTURE, message};
        }

    } catch (OracleException& e) {
        auto message = string( "Invalid oracle spec in submitOracleRequest " ) + e.what();
        LOG( err, message );
        return {e.getError(), message};
    } catch ( exception& e ) {
        auto message = string( "Invalid oracle spec in submitOracleRequest " ) + e.what();
        LOG( err, message );
        return {ORACLE_INVALID_JSON_REQUEST, message};
    } catch ( ... ) {
        auto message = string( "Unknown exception in " ) + __FUNCTION__ ;
        LOG( err, message);
        return { ORACLE_INTERNAL_SERVER_ERROR, message};
    }

    try {
        _receipt = msg->getParsedSpec()->getReceipt();
        if ( _receipt.empty() ) {
            auto message = "Could not compute oracle receipt ";
            LOG( err, message);
            return {ORACLE_INTERNAL_SERVER_ERROR, message};
        }
    } catch ( exception& e ) {
        auto message = string( "Exception computing receipt " ) + e.what();
        LOG( err, message);
        return {ORACLE_INTERNAL_SERVER_ERROR, message};
    } catch ( ... ) {
        auto message = string( "Unknown Exception computing receipt " );
        LOG( err, message);
        return {ORACLE_INTERNAL_SERVER_ERROR, message};
    }

    try {
        return {broadcastRequest( msg ), ""};
    } catch ( exception& e ) {
        auto message = string( "Exception broadcasting message " ) + e.what();
        LOG( err, message);
        return {ORACLE_INTERNAL_SERVER_ERROR, message};
    } catch ( ... ) {
        auto message = "Internal server error in submitOracleRequest ";
        LOG( err, message);
        return {ORACLE_INTERNAL_SERVER_ERROR, message};
    }
}


uint64_t OracleClient::checkOracleResult( const string& _receipt, string& _result ) {
    try {
        auto oracleReceivedResults = receiptsMap.getIfExists( _receipt );

        if ( !oracleReceivedResults.has_value() ) {
            LOG( warn, "Received tryGettingOracleResult  with unknown receipt" + _receipt );
            return ORACLE_UNKNOWN_RECEIPT;
        }

        auto receipts = std::any_cast< ptr< OracleReceivedResults > >( oracleReceivedResults );

        return receipts->tryGettingResult( _result );

    } catch ( exception& e ) {
        LOG( err, string( "Exception broadcasting message " ) + e.what() );
        return ORACLE_INTERNAL_SERVER_ERROR;
    } catch ( ... ) {
        LOG( err, "Internal server error in submitOracleRequest " );
        return ORACLE_INTERNAL_SERVER_ERROR;
    }
}


void OracleClient::processResponseMessage( const ptr< MessageEnvelope >& _me ) {
    try {
        CHECK_STATE( _me );

        auto origin = ( uint64_t ) _me->getSrcSchainIndex();

        CHECK_STATE( origin > 0 || origin <= getSchain()->getNodeCount() );

        auto msg = dynamic_pointer_cast< OracleResponseMessage >( _me->getMessage() );

        CHECK_STATE( msg );

        auto receipt = msg->getReceipt();

        auto receivedResults = receiptsMap.getIfExists( receipt );

        if ( !receivedResults.has_value() ) {
            LOG( warn, "Received OracleResponseMessage with unknown receipt" + receipt );
            return;
        }

        auto rslts = std::any_cast< ptr< OracleReceivedResults > >( receivedResults );
        rslts->insertIfDoesntExist(
            origin, msg->getOracleResult( rslts->getRequestSpec(), getSchain()->getSchainID() ) );
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}
