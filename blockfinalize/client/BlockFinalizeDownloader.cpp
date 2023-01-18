/*
    Copyright (C) 2019- SKALE Labs

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

    @file BlockFinalizeDownloader.cpp
    @author Stan Kladko
    @date 2019-
*/

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

    @file BlockFinalizeDownloader.cpp
    @author Stan Kladko
    @date 2018
*/

#include <exceptions/ConnectionRefusedException.h>

#include "exceptions/ExitRequestedException.h"


#include "abstracttcpserver/ConnectionStatus.h"

#include "chains/TestConfig.h"
#include "network/ClientSocket.h"
#include "network/IO.h"
#include "network/Network.h"
#include "node/Node.h"

#include "chains/Schain.h"
#include "crypto/TrivialSignature.h"
#include "datastructures/BlockProposalFragment.h"

#include "datastructures/CommittedBlock.h"
#include "db/BlockProposalDB.h"
#include "db/DAProofDB.h"
#include "headers/BlockFinalizeRequestHeader.h"
#include "headers/BlockProposalRequestHeader.h"
#include "monitoring/LivelinessMonitor.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "utils/Time.h"

#include "BlockFinalizeDownloader.h"
#include "BlockFinalizeDownloaderThreadPool.h"
#include "crypto/CryptoManager.h"


BlockFinalizeDownloader::BlockFinalizeDownloader(
    Schain* _sChain, block_id _blockId, schain_index _proposerIndex )
    : Agent( *_sChain, false, true ),
      blockId( _blockId ),
      proposerIndex( _proposerIndex ),
      fragmentList( _blockId, ( uint64_t ) _sChain->getNodeCount() - 1 ) {

    CHECK_ARGUMENT( _sChain )

    CHECK_STATE( _sChain->getNodeCount() > 1 )

    if (_proposerIndex == _sChain->getSchainIndex()) {
        LOG(err, "Finalizing own proposal");
    }

    try {
        logThreadLocal_ = _sChain->getNode()->getLog();

        CHECK_STATE( sChain )

    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( FatalError( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


nlohmann::json BlockFinalizeDownloader::readBlockFinalizeResponseHeader(
    const ptr< ClientSocket >& _socket ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ )
    CHECK_ARGUMENT( _socket )
    return getSchain()->getIo()->readJsonHeader(
        _socket->getDescriptor(), "Read BlockFinalize response", 10, _socket->getIP() );
}


uint64_t BlockFinalizeDownloader::downloadFragment(
    schain_index _dstIndex, fragment_index _fragmentIndex ) {
    LOG( debug, "BLCK_FRG_DWNLD:" + to_string( _fragmentIndex ) + ":" + to_string( _dstIndex ) );

    try {
        auto header = make_shared< BlockFinalizeRequestHeader >(
            *sChain, blockId, proposerIndex, this->getNode()->getNodeID(), _fragmentIndex );
        CHECK_STATE( _dstIndex != ( uint64_t ) getSchain()->getSchainIndex() )
        if ( getSchain()->getDeathTimeMs( ( uint64_t ) _dstIndex ) + NODE_DEATH_INTERVAL_MS >
             Time::getCurrentTimeMs() ) {
            usleep( 100000 );  // emulate timeout
            BOOST_THROW_EXCEPTION( ConnectionRefusedException(
                "Dead node:" + to_string( _dstIndex ), 5, __CLASS_NAME__ ) );
        }
        auto socket = make_shared< ClientSocket >( *sChain, _dstIndex, CATCHUP );

        auto io = getSchain()->getIo();

        try {
            io->writeMagic( socket );
        } catch ( ExitRequestedException& ) {
            throw;
        } catch ( ... ) {
            throw_with_nested( NetworkProtocolException(
                "BlockFinalizec: Server disconnect sending magic", __CLASS_NAME__ ) );
        }

        try {
            io->writeHeader( socket, header );
        } catch ( ExitRequestedException& ) {
            throw;
        } catch ( ... ) {
            auto errString = "BlockFinalizec step 1: can not write BlockFinalize request";
            LOG( err, errString );
            throw_with_nested( NetworkProtocolException( errString, __CLASS_NAME__ ) );
        }

        nlohmann::json response;

        try {
            response = readBlockFinalizeResponseHeader( socket );
        } catch ( ExitRequestedException& ) {
            throw;
        } catch ( ... ) {
            auto errString = "BlockFinalizec step 2: can not read BlockFinalize response";
            LOG( err, errString );
            throw_with_nested( NetworkProtocolException( errString, __CLASS_NAME__ ) );
        }


        auto status = ( ConnectionStatus ) Header::getUint64( response, "status" );

        if ( status == CONNECTION_DISCONNECT ) {
            LOG( debug, "BLCK_FRG_DWNLD:NO_FRG:" + to_string( _fragmentIndex ) + ":" +
                           to_string( _dstIndex ) );
            return 0;
        }


        if ( status != CONNECTION_PROCEED ) {
            BOOST_THROW_EXCEPTION( NetworkProtocolException(
                "Server error in BlockFinalize response:" + to_string( status ), __CLASS_NAME__ ) );
        }


        ptr< BlockProposalFragment > blockFragment;

        try {
            blockFragment =
                readBlockFragment( socket, response, _fragmentIndex, getSchain()->getNodeCount() );
            CHECK_ARGUMENT( blockFragment )
        } catch ( ExitRequestedException& ) {
            throw;
        } catch ( ... ) {
            auto errString = "BlockFinalizec step 3: can not read fragment";
            LOG( err, errString );
            throw_with_nested( NetworkProtocolException( errString, __CLASS_NAME__ ) );
        }


        uint64_t next = 0;

        fragmentList.addFragment( blockFragment, next );

        return next;

    } catch ( ExitRequestedException& e ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

uint64_t BlockFinalizeDownloader::readFragmentSize( nlohmann::json _responseHeader ) {
    uint64_t result = Header::getUint64( _responseHeader, "fragmentSize" );

    if ( result == 0 ) {
        BOOST_THROW_EXCEPTION( NetworkProtocolException( "fragmentSize == 0", __CLASS_NAME__ ) );
    }

    return result;
}

uint64_t BlockFinalizeDownloader::readBlockSize( nlohmann::json _responseHeader ) {
    uint64_t result = Header::getUint64( _responseHeader, "blockSize" );

    if ( result == 0 ) {
        BOOST_THROW_EXCEPTION( NetworkProtocolException( "blockSize == 0", __CLASS_NAME__ ) );
    }

    return result;
}

string BlockFinalizeDownloader::readBlockHash( nlohmann::json _responseHeader ) {
    auto result = Header::getString( _responseHeader, "blockHash" );
    return result;
}

string BlockFinalizeDownloader::readDAProofSig( nlohmann::json _responseHeader ) {
    if (getSchain()->verifyDASigsPatch(getSchain()->getLastCommittedBlockTimeStamp().getS())) {
        return Header::getString( _responseHeader, "daSig" );
    } else {
        return Header::maybeGetString( _responseHeader, "daSig" );
    }
}


ptr< BlockProposalFragment > BlockFinalizeDownloader::readBlockFragment(
    const ptr< ClientSocket >& _socket, nlohmann::json _responseHeader,
    fragment_index _fragmentIndex, node_count _nodeCount ) {
    CHECK_ARGUMENT( _socket )

    CHECK_ARGUMENT( _responseHeader > 0 )

    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    auto fragmentSize = readFragmentSize( _responseHeader );
    auto blockSize = readBlockSize( _responseHeader );
    auto h = readBlockHash( _responseHeader );
    CHECK_STATE(!h.empty())
    auto sig = readDAProofSig( _responseHeader );

    {
        LOCK( m )


        // if we did not receive block hash yet, set it. Otherwise, compare it to the known hash
        if (this->blockHash.empty()) {
            this->blockHash = h;
        } else {
            CHECK_STATE( h == blockHash );
        }

        // if we did not received da sig yet, set it.
        if ( !this->daSig && !sig.empty()) {
            auto blakeHash = BLAKE3Hash::fromHex( h );
            this->daSig = getSchain()->getCryptoManager()->verifyDAProofThresholdSig( blakeHash, sig, blockId );
        }

    }

    auto serializedFragment = make_shared< vector< uint8_t > >( fragmentSize );

    try {
        getSchain()->getIo()->readBytes(
            _socket->getDescriptor(), serializedFragment, msg_len( fragmentSize ), 30 );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( NetworkProtocolException( "Could not read blocks", __CLASS_NAME__ ) );
    }

    ptr< BlockProposalFragment > fragment = nullptr;

    try {
        fragment = make_shared< BlockProposalFragment >( blockId, ( uint64_t ) _nodeCount - 1,
            _fragmentIndex, serializedFragment, blockSize, blockHash );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested(
            NetworkProtocolException( "Could not parse block fragment", __CLASS_NAME__ ) );
    }

    return fragment;
}


void BlockFinalizeDownloader::workerThreadFragmentDownloadLoop(
    BlockFinalizeDownloader* _agent, schain_index _dstIndex ) {
    CHECK_STATE( _agent )


    auto sChain = _agent->getSchain();
    auto node = sChain->getNode();
    auto proposalDB = node->getBlockProposalDB();
    auto daProofDB = node->getDaProofDB();
    auto blockId = _agent->getBlockId();
    auto proposerIndex = _agent->getProposerIndex();
    auto sChainIndex = sChain->getSchainIndex();
    bool testFinalizationDownloadOnly = node->getTestConfig()->isFinalizationDownloadOnly();

    setThreadName( "BlckFinLoop", node->getConsensusEngine() );

    node->waitOnGlobalClientStartBarrier();
    if ( node->isExitRequested() )
        return;

    // since the node does not download from itself
    // and since the number of fragment is one less the number of
    // nodes, nodes that have sChainIndex more than current node, download _dstNodeIndex - 1
    // fragment

    uint64_t nextFragment;

    if ( _dstIndex > ( uint64_t ) sChainIndex ) {
        nextFragment = ( uint64_t ) _dstIndex - 1;
    } else {
        nextFragment = ( uint64_t ) _dstIndex;
    }

    try {
        while ( !node->isExitRequested() && !_agent->fragmentList.isComplete() ) {
            if ( !testFinalizationDownloadOnly ) {
                // take into account that the block can
                //  be in parallel committed through catchup
                if ( sChain->getLastCommittedBlockID() >= blockId ) {
                    return;
                }

                // take into account that the proposal and da proof can arrive through
                // BlockproposalServerAgent

                if ( proposalDB->proposalExists( blockId, proposerIndex ) ) {
                    auto proposal =
                        proposalDB->getBlockProposal( _agent->blockId, _agent->proposerIndex );
                    CHECK_STATE( proposal )
                    if ( daProofDB->haveDAProof( proposal ) ) {
                        return;
                    }
                }
            }

            try {
                nextFragment = _agent->downloadFragment( _dstIndex, nextFragment );
                if ( nextFragment == 0 ) {
                    // all fragments have been downloaded
                    return;
                }
            } catch ( ExitRequestedException& ) {
                return;
            } catch ( ConnectionRefusedException& e ) {
                _agent->logConnectionRefused( e, _dstIndex );
                if ( _agent->fragmentList.isComplete() )
                    return;
                usleep( static_cast< __useconds_t >( node->getWaitAfterNetworkErrorMs() * 1000 ) );
            } catch ( exception& e ) {
                SkaleException::logNested( e );
                if ( _agent->fragmentList.isComplete() )
                    return;
                usleep( static_cast< __useconds_t >( node->getWaitAfterNetworkErrorMs() * 1000 ) );
            }
        }
    } catch ( FatalError& e ) {
        SkaleException::logNested( e );
        node->exitOnFatalError( e.what() );
    }
}

ptr< BlockProposal > BlockFinalizeDownloader::downloadProposal() {
    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    {
        threadPool = make_shared< BlockFinalizeDownloaderThreadPool >(
            ( uint64_t ) getSchain()->getNodeCount(), this );
        threadPool->startService();
        threadPool->joinAll();
    }

    try {
        if ( fragmentList.isComplete() ) {
            auto block = BlockProposal::deserialize(
                fragmentList.serialize(), getSchain()->getCryptoManager(), true );
            CHECK_STATE( block )
            CHECK_STATE( block->getProposerIndex() == ( uint64_t ) proposerIndex );
            {
                LOCK( m )
                if ( !this->blockHash.empty() ) {
                    auto h = BLAKE3Hash::fromHex( blockHash );
                    CHECK_STATE2( block->getHash().compare( h ) == 0, "Incorrect block hash" );
                }
            }
            return block;
        } else {
            return nullptr;
        }
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( exception& e ) {
        SkaleException::logNested( e );
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

BlockFinalizeDownloader::~BlockFinalizeDownloader() {}

block_id BlockFinalizeDownloader::getBlockId() {
    return blockId;
}

schain_index BlockFinalizeDownloader::getProposerIndex() {
    return proposerIndex;
}

ptr<ThresholdSignature> BlockFinalizeDownloader::getDaSig(uint64_t _timeStampS)  {

    if (getSchain()->verifyDASigsPatch(_timeStampS))
        CHECK_STATE2(daSig, "BlockFinalizeDownloader: block did not include DA sig:"
                                 + to_string(_timeStampS));

    if (daSig)
        return daSig;
    else
        return make_shared<TrivialSignature>(getBlockId(), getSchain()->getTotalSigners(),
                                             getSchain()->getRequiredSigners());
}
