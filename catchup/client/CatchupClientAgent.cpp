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

    @file CatchupClientAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"

#include "Log.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/FatalError.h"

#include "thirdparty/json.hpp"

#include "abstracttcpserver/ConnectionStatus.h"

#include "chains/Schain.h"
#include "datastructures/CommittedBlockList.h"
#include "exceptions/ConnectionRefusedException.h"
#include "exceptions/NetworkProtocolException.h"
#include "headers/CatchupRequestHeader.h"
#include "headers/CatchupResponseHeader.h"
#include "network/ClientSocket.h"
#include "network/IO.h"
#include "network/Network.h"
#include "pendingqueue/PendingTransactionsAgent.h"

#include "CatchupClientAgent.h"
#include "CatchupClientThreadPool.h"


CatchupClientAgent::CatchupClientAgent( Schain& _sChain ) : Agent( _sChain, false ) {
    try {
        logThreadLocal_ = _sChain.getNode()->getLog();
        this->sChain = &_sChain;

        if ( _sChain.getNodeCount() > 1 ) {
            this->catchupClientThreadPool = make_shared< CatchupClientThreadPool >( 1, this );
            catchupClientThreadPool->startService();
        }
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( FatalError( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


nlohmann::json CatchupClientAgent::readCatchupResponseHeader(const ptr< ClientSocket >& _socket ) {
    CHECK_ARGUMENT( _socket );
    auto result =
        sChain->getIo()->readJsonHeader( _socket->getDescriptor(), "Read catchup response",
            MAX_CATCHUP_DOWNLOAD_BYTES);
    return result;
}


void CatchupClientAgent::sync( schain_index _dstIndex ) {
    LOG( debug, "Catchupc step 0: requesting blocks after " +
                    to_string( getSchain()->getLastCommittedBlockID() ) );

    auto header = make_shared< CatchupRequestHeader >( *sChain, _dstIndex );
    auto socket = make_shared< ClientSocket >( *sChain, _dstIndex, CATCHUP );
    auto io = getSchain()->getIo();
    CHECK_STATE( io );

    try {
        io->writeMagic( socket );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( NetworkProtocolException(
            "Catchupc: Server disconnect sending magic", __CLASS_NAME__ ) );
    }

    try {
        io->writeHeader( socket, header );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        auto errString = "Catchupc step 1: can not write catchup request";
        LOG( debug, errString );
        throw_with_nested( NetworkProtocolException( errString, __CLASS_NAME__ ) );
    }
    LOG( debug, "Catchupc step 1: wrote catchup request" );

    nlohmann::json response;

    try {
        response = readCatchupResponseHeader( socket );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        auto errString = "Catchupc step 2: can not read catchup response";
        LOG( debug, errString );
        throw_with_nested( NetworkProtocolException( errString, __CLASS_NAME__ ) );
    }


    LOG( debug, "Catchupc step 2: read catchup response header" );

    auto status = ( ConnectionStatus ) Header::getUint64( response, "status" );

    if ( status == CONNECTION_DISCONNECT ) {
        LOG( debug, "Catchupc got response::no missing blocks" );
        return;
    }


    if ( status != CONNECTION_PROCEED ) {
        BOOST_THROW_EXCEPTION( NetworkProtocolException(
            "Server error in catchup response:" + to_string( status ), __CLASS_NAME__ ) );
    }


    ptr< CommittedBlockList > blocks;


    try {
        blocks = readMissingBlocks( socket, response );
        CHECK_STATE( blocks );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        auto errString = "Catchupc step 3: can not read missing blocks";
        LOG( err, errString );
        throw_with_nested( NetworkProtocolException( errString, __CLASS_NAME__ ) );
    }

    LOG( debug, "Catchupc step 3: got missing blocks:" + to_string( blocks->getBlocks()->size() ) );

    getSchain()->blockCommitsArrivedThroughCatchup( blocks );
    LOG( debug, "Catchupc success" );
}

size_t CatchupClientAgent::parseBlockSizes(
    nlohmann::json _responseHeader, const ptr<vector<uint64_t>>& _blockSizes ) {
    nlohmann::json jsonSizes = _responseHeader["sizes"];

    CHECK_ARGUMENT( _blockSizes );


    if ( !jsonSizes.is_array() ) {
        BOOST_THROW_EXCEPTION(
            NetworkProtocolException( "JSON Sizes is not an array ", __CLASS_NAME__ ) );
    }


    if ( jsonSizes.size() == 0 ) {
        BOOST_THROW_EXCEPTION( NetworkProtocolException( "JSON sizes is empty", __CLASS_NAME__ ) );
    }

    size_t totalSize = 0;

    for ( auto&& size : jsonSizes ) {
        _blockSizes->push_back( size );
        totalSize += ( size_t ) size;
    }

    if ( totalSize < 4 ) {
        BOOST_THROW_EXCEPTION( NetworkProtocolException( "TotalSize < 4", __CLASS_NAME__ ) );
    }


    if ( totalSize > getNode()->getMaxCatchupDownloadBytes() ) {
        BOOST_THROW_EXCEPTION( NetworkProtocolException(
            "totalSize > getNode()->getMaxCatchupDownloadBytes()", __CLASS_NAME__ ) );
    }

    return totalSize + 2;
};


ptr< CommittedBlockList > CatchupClientAgent::readMissingBlocks(
    ptr< ClientSocket >& _socket, nlohmann::json responseHeader ) {
    CHECK_ARGUMENT( responseHeader > 0 );
    CHECK_ARGUMENT( _socket );

    auto blockSizes = make_shared< vector< uint64_t > >();

    auto totalSize = parseBlockSizes( responseHeader, blockSizes );

    auto serializedBlocks = make_shared<vector<uint8_t>>( totalSize );

    try {
        getSchain()->getIo()->readBytes(
            _socket->getDescriptor(), serializedBlocks, msg_len( totalSize ) );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( NetworkProtocolException( "Could not read blocks", __CLASS_NAME__ ) );
    }

    if ( serializedBlocks->at( 0 ) != '[' ) {
        BOOST_THROW_EXCEPTION(
            NetworkProtocolException( "Serialized blocks do not start with [", __CLASS_NAME__ ) );
    }


    ptr< CommittedBlockList > blockList = nullptr;

    try {
        blockList = CommittedBlockList::deserialize(
            getSchain()->getCryptoManager(), blockSizes, serializedBlocks, 0 );
        CHECK_STATE( blockList );
    } catch ( ExitRequestedException& ) { throw; } catch ( ... ) {
        throw_with_nested(
            NetworkProtocolException( "Could not parse block list", __CLASS_NAME__ ) );
    }

    return blockList;
}


void CatchupClientAgent::workerThreadItemSendLoop( CatchupClientAgent* _agent ) {
    setThreadName( "CatchupClient", _agent->getNode()->getConsensusEngine() );

    CHECK_ARGUMENT( _agent );

    _agent->waitOnGlobalStartBarrier();

    auto destinationSchainIndex = schain_index( 1 );

    try {
        while ( !_agent->getSchain()->getNode()->isExitRequested() ) {
            std::this_thread::sleep_for(std::chrono::milliseconds(
                                        _agent->getNode()->getCatchupIntervalMs()
                                        ));

            try {
                _agent->sync( destinationSchainIndex );
            } catch ( ExitRequestedException& ) {
                return;
            } catch ( ConnectionRefusedException& e ) {
                _agent->logConnectionRefused( e, destinationSchainIndex );
            } catch ( exception& e ) {
                SkaleException::logNested( e );
            }

            destinationSchainIndex = nextSyncNodeIndex( _agent, destinationSchainIndex );
        };
    } catch ( FatalError& e ) {
        _agent->getNode()->exitOnFatalError( e.getMessage() );
    }
}

schain_index CatchupClientAgent::nextSyncNodeIndex(
    const CatchupClientAgent* _agent, schain_index _destinationSchainIndex ) {

    CHECK_ARGUMENT( _agent );

    auto nodeCount = ( uint64_t ) _agent->getSchain()->getNodeCount();

    auto index = _destinationSchainIndex - 1;

    do {
        index = ( ( uint64_t ) index + 1 ) % nodeCount;
    } while ( index == ( _agent->getSchain()->getSchainIndex() - 1 ) );

    return index + 1;
}
