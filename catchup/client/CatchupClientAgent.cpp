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
#include "crypto/CryptoManager.h"

#include "CatchupClientAgent.h"
#include "CatchupClientThreadPool.h"
#include "chains/Schain.h"
#include "datastructures/CommittedBlock.h"
#include "datastructures/CommittedBlockList.h"
#include "exceptions/ConnectionRefusedException.h"
#include "exceptions/InvalidSignatureException.h"
#include "exceptions/NetworkProtocolException.h"
#include "headers/CatchupRequestHeader.h"
#include "headers/CatchupResponseHeader.h"
#include "network/ClientSocket.h"
#include "network/IO.h"
#include "network/Network.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "sys/random.h"
#include "utils/Time.h"


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


nlohmann::json CatchupClientAgent::readCatchupResponseHeader(
    const ptr< ClientSocket >& _socket, ptr< CatchupRequestHeader > _requestHeader ) {
    CHECK_ARGUMENT( _socket )
    CHECK_ARGUMENT( _requestHeader )
    auto result = sChain->getIo()->readJsonHeader( _socket->getDescriptor(),
        "Read catchup response", 30, _socket->getIP(), MAX_CATCHUP_DOWNLOAD_BYTES );
    return result;
}


[[nodiscard]] uint64_t CatchupClientAgent::sync( schain_index _dstIndex ) {
    LOG( debug, "Catchupc step 0: requesting blocks after " +
                    to_string( getSchain()->getLastCommittedBlockID() ) );

    auto requestHeader = make_shared< CatchupRequestHeader >( *sChain, _dstIndex );
    CHECK_STATE( _dstIndex != ( uint64_t ) getSchain()->getSchainIndex() )

    if ( getSchain()->getDeathTimeMs( ( uint64_t ) _dstIndex ) + NODE_DEATH_INTERVAL_MS >
         Time::getCurrentTimeMs() ) {
        usleep( 100000 );
        throw ConnectionRefusedException(
            "Connecting to dead node " + to_string( _dstIndex ), 5, __CLASS_NAME__ );
    }
    auto socket = make_shared< ClientSocket >( *sChain, _dstIndex, CATCHUP );
    auto io = getSchain()->getIo();
    CHECK_STATE( io )

    try {
        io->writeMagic( socket );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( NetworkProtocolException(
            "Catchupc: Server disconnect sending magic", __CLASS_NAME__ ) );
    }

    try {
        io->writeHeader( socket, requestHeader );
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
        response = readCatchupResponseHeader( socket, requestHeader );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        auto errString = "Catchupc step 2: can not read catchup response";
        LOG( debug, errString );
        throw_with_nested( NetworkProtocolException( errString, __CLASS_NAME__ ) );
    }


    LOG( debug, "Catchupc step 2: read catchup response requestHeader" );

    auto status = ( ConnectionStatus ) Header::getUint64( response, "status" );

    if ( status == CONNECTION_DISCONNECT ) {
        LOG( debug, "Catchupc got response::no missing blocks" );
        return 0;
    }


    if ( status != CONNECTION_PROCEED ) {
        BOOST_THROW_EXCEPTION( NetworkProtocolException(
            "Server error in catchup response:" + to_string( status ), __CLASS_NAME__ ) );
    }


    ptr< CommittedBlockList > blocks;


    try {
        blocks = readMissingBlocks( socket, response, requestHeader );

        CHECK_STATE( blocks )
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        auto errString = "Catchupc step 3: can not read missing blocks";
        LOG( err, errString );
        throw_with_nested( NetworkProtocolException( errString, __CLASS_NAME__ ) );
    }

    LOG( debug, "Catchupc step 3: got missing blocks:" + to_string( blocks->getBlocks()->size() ) );

    auto result = getSchain()->blockCommitsArrivedThroughCatchup( blocks );
    LOG( debug, "Catchupc success" );
    return result;
}

size_t CatchupClientAgent::parseBlockSizes( nlohmann::json _responseHeader,
    const ptr< vector< uint64_t > >& _blockSizes, ptr< CatchupRequestHeader > _requestHeader ) {
    if ( _responseHeader.count( "sizes" ) == 0 ) {
        LOG( err, "Invalid response header:" + _responseHeader.dump() );
        BOOST_THROW_EXCEPTION(
            NetworkProtocolException( "No json sizes element in response", __CLASS_NAME__ ) );
    }

    nlohmann::json jsonSizes = _responseHeader["sizes"];

    CHECK_ARGUMENT( _blockSizes )


    if ( !jsonSizes.is_array() ) {
        LOG( err, "Invalid catchup response header:" + _responseHeader.dump() );
        LOG( err, "Corresponding request:" + _requestHeader->serializeToString() );
        BOOST_THROW_EXCEPTION(
            NetworkProtocolException( "JSON Sizes is not an array ", __CLASS_NAME__ ) );
    }


    if ( jsonSizes.size() == 0 ) {
        LOG( err, "Invalid catchup response header:" + _responseHeader.dump() );
        LOG( err, "Corresponding request:" + _requestHeader->serializeToString() );
        BOOST_THROW_EXCEPTION( NetworkProtocolException( "JSON sizes is empty", __CLASS_NAME__ ) );
    }

    size_t totalSize = 0;

    for ( auto&& size : jsonSizes ) {
        _blockSizes->push_back( size );
        totalSize += ( size_t ) size;
    }

    if ( totalSize < 4 ) {
        LOG( err, "Invalid catchup response header:" + _responseHeader.dump() );
        LOG( err, "Corresponding request:" + _requestHeader->serializeToString() );
        BOOST_THROW_EXCEPTION( NetworkProtocolException( "TotalSize < 4", __CLASS_NAME__ ) );
    }


    if ( totalSize > getNode()->getMaxCatchupDownloadBytes() ) {
        LOG( err, "Invalid response header:" + _responseHeader.dump() );
        LOG( err, "Corresponding request:" + _requestHeader->serializeToString() );
        BOOST_THROW_EXCEPTION( NetworkProtocolException(
            "totalSize > getNode()->getMaxCatchupDownloadBytes()", __CLASS_NAME__ ) );
    }

    return totalSize + 2;
}


ptr< CommittedBlockList > CatchupClientAgent::readMissingBlocks( ptr< ClientSocket >& _socket,
    nlohmann::json& _responseHeader, ptr< CatchupRequestHeader > _requestHeader ) {
    CHECK_ARGUMENT( _responseHeader > 0 )
    CHECK_ARGUMENT( _socket )
    CHECK_ARGUMENT( _requestHeader )

    auto blockSizes = make_shared< vector< uint64_t > >();

    auto totalSize = parseBlockSizes( _responseHeader, blockSizes , _requestHeader);

    auto serializedBlocks = make_shared< vector< uint8_t > >( totalSize );

    try {
        getSchain()->getIo()->readBytes(
            _socket->getDescriptor(), serializedBlocks, msg_len( totalSize ), 30 );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( NetworkProtocolException( "Could not read blocks", __CLASS_NAME__ ) );
    }


    ptr< CommittedBlockList > blockList = nullptr;

    try {
        blockList = CommittedBlockList::deserialize(
            getSchain()->getCryptoManager(), blockSizes, serializedBlocks, 0 );
        CHECK_STATE( blockList )


        if ( blockSizes->size() > 1 ) {
            LOG(
                info, "CATCHUP_GOT_BLOCKS:COUNT:" + to_string( blockSizes->size() ) +
                          ":STARTBLOCK:" + string( blockList->getBlocks()->at( 0 )->getBlockID() ) +
                          ":FROM_NODE:" + _socket->getIP() );
        }
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested(
            NetworkProtocolException( "Could not process block list", __CLASS_NAME__ ) );
    }

    return blockList;
}


void CatchupClientAgent::workerThreadItemSendLoop( CatchupClientAgent* _agent ) {
    setThreadName( "CatchupClient", _agent->getNode()->getConsensusEngine() );

    CHECK_ARGUMENT( _agent )

    _agent->waitOnGlobalStartBarrier();

    // wait until the schain state is fully initialized
    // otherwise the chain can not accept catchup blocks
    while ( !_agent->getSchain()->getIsStateInitialized() ) {
        usleep( 100 * 1000 );
    }


    // start with a random index and then to round-robin


    auto nodeCount = ( uint64_t ) _agent->getSchain()->getNodeCount();
    auto selfIndex = _agent->getSchain()->getSchainIndex();

    uint64_t startIndex;

    auto lastBlockCount = 0;

    do {
        uint64_t random;
        getrandom( &random, sizeof( random ), 0 );
        startIndex = random % nodeCount + 1;
    } while ( startIndex == ( uint64_t ) selfIndex );

    auto destinationSchainIndex = schain_index( startIndex );

    try {
        while ( !_agent->getSchain()->getNode()->isExitRequested() ) {
            // sleep if previous iteration did not result in blocks
            if ( lastBlockCount == 0 )
                std::this_thread::sleep_for(
                    std::chrono::milliseconds( _agent->getNode()->getCatchupIntervalMs() ) );

            try {
                lastBlockCount = _agent->sync( destinationSchainIndex );
            } catch ( ExitRequestedException& ) {
                return;
            } catch ( ConnectionRefusedException& e ) {
                _agent->logConnectionRefused( e, destinationSchainIndex );
            } catch ( exception& e ) {
                SkaleException::logNested( e );
            }

            destinationSchainIndex = nextSyncNodeIndex( _agent, destinationSchainIndex );
        }
    } catch ( FatalError& e ) {
        SkaleException::logNested( e );
        _agent->getNode()->exitOnFatalError( e.what() );
    }
}

schain_index CatchupClientAgent::nextSyncNodeIndex(
    const CatchupClientAgent* _agent, schain_index _destinationSchainIndex ) {
    CHECK_ARGUMENT( _agent )

    auto nodeCount = ( uint64_t ) _agent->getSchain()->getNodeCount();

    auto index = _destinationSchainIndex - 1;

    do {
        index = ( ( uint64_t ) index + 1 ) % nodeCount;
    } while ( index == ( _agent->getSchain()->getSchainIndex() - 1 ) );

    return index + 1;
}
