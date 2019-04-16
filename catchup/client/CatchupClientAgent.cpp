#include "../../SkaleConfig.h"

#include "../../Log.h"
#include "../../exceptions/ExitRequestedException.h"
#include "../../exceptions/FatalError.h"

#include "../../thirdparty/json.hpp"

#include "../../abstracttcpserver/ConnectionStatus.h"

#include "../../network/ClientSocket.h"
#include "../../network/IO.h"
#include "../../network/TransportNetwork.h"
#include "../../node/Node.h"

#include "../../crypto/SHAHash.h"
#include "../../chains/Schain.h"
#include "../../datastructures/CommittedBlockList.h"
#include "../../exceptions/NetworkProtocolException.h"
#include "../../headers/BlockProposalHeader.h"
#include "../../headers/CatchupRequestHeader.h"
#include "../../headers/CatchupResponseHeader.h"
#include "../../pendingqueue/PendingTransactionsAgent.h"

#include "CatchupClientAgent.h"
#include "CatchupClientThreadPool.h"


CatchupClientAgent::CatchupClientAgent( Schain& _sChain ) : Agent( _sChain, false ) {
    logThreadLocal_ = _sChain.getNode()->getLog();
    this->sChain = &_sChain;
    threadCounter = 0;

    if ( _sChain.getNodeCount() > 1 ) {
        this->catchupClientThreadPool = make_shared< CatchupClientThreadPool >( 1, this );
        catchupClientThreadPool->startService();
    }
}


nlohmann::json CatchupClientAgent::readCatchupResponseHeader( ptr< ClientSocket > _socket ) {
    return sChain->getIo()->readJsonHeader( _socket->getDescriptor(), "Read catchup response" );
}


void CatchupClientAgent::sync( schain_index _dstIndex ) {
    LOG( debug,
        "Catchupc step 0: request for block" + to_string( getSchain()->getCommittedBlockID() ) );

    auto header = make_shared< CatchupRequestHeader >( *sChain, _dstIndex );
    auto socket = make_shared< ClientSocket >( *sChain, _dstIndex, CATCHUP );
    auto io = getSchain()->getIo();


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
    nlohmann::json _responseHeader, ptr< vector< size_t > > _blockSizes ) {
    nlohmann::json jsonSizes = _responseHeader["sizes"];

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

    return totalSize;
};


ptr< CommittedBlockList > CatchupClientAgent::readMissingBlocks(
    ptr< ClientSocket > _socket, nlohmann::json responseHeader ) {
    ASSERT( responseHeader > 0 );

    auto blockSizes = make_shared< vector< size_t > >();

    auto totalSize = parseBlockSizes( responseHeader, blockSizes );

    auto serializedBlocks = make_shared< vector< uint8_t > >( totalSize );

    try {
        getSchain()->getIo()->readBytes( _socket->getDescriptor(),
            ( in_buffer* ) serializedBlocks->data(), msg_len( totalSize ) );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( NetworkProtocolException( "Could not read blocks", __CLASS_NAME__ ) );
    }

    if ( ( *serializedBlocks )[sizeof( uint64_t )] != '{' ) {
        throw_with_nested( NetworkProtocolException(
            "First serialized block does not start with {", __CLASS_NAME__ ) );
    }


    ptr< CommittedBlockList > blockList = nullptr;

    try {
        blockList = make_shared< CommittedBlockList >( blockSizes, serializedBlocks );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested(
            NetworkProtocolException( "Could not parse block list", __CLASS_NAME__ ) );
    }

    return blockList;
}


void CatchupClientAgent::workerThreadItemSendLoop( CatchupClientAgent* agent ) {
    setThreadName( __CLASS_NAME__ );

    agent->waitOnGlobalStartBarrier();

    auto destinationSubChainIndex = schain_index( 0 );

    try {
        while ( !agent->getSchain()->getNode()->isExitRequested() ) {
            usleep( agent->getNode()->getCatchupIntervalMs() * 1000 );

            try {
                agent->sync( destinationSubChainIndex );
            } catch ( ExitRequestedException& ) {
                return;
            } catch ( Exception& e ) {
                Exception::log_exception( e );
            }

            destinationSubChainIndex = nextSyncNodeIndex( agent, destinationSubChainIndex );
        };
    } catch ( FatalError* e ) {
        agent->getNode()->exitOnFatalError( e->getMessage() );
    }
}

schain_index CatchupClientAgent::nextSyncNodeIndex(
    const CatchupClientAgent* agent, schain_index _destinationSubChainIndex ) {
    auto nodeCount = ( uint64_t ) agent->getSchain()->getNodeCount();

    auto index = _destinationSubChainIndex;

    do {
        index = ( ( uint64_t ) index + 1 ) % nodeCount;
    } while ( index == agent->getSchain()->getSchainIndex() );
    return index;
}
