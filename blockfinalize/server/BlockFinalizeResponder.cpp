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
*/

#include "SkaleCommon.h"
#include "Log.h"

#include "blockfinalize/server/BlockFinalizeResponder.h"

#include "abstracttcpserver/ConnectionStatus.h"
#include "chains/Schain.h"
#include "crypto/BLAKE3Hash.h"
#ifdef BITE
#include "crypto/AESKeyDecryptionShareList.h"
#include "db/TEDecryptionDB.h"
#endif
#include "datastructures/BlockProposalFragment.h"
#include "datastructures/CommittedBlock.h"
#include "db/BlockProposalDB.h"
#include "db/DAProofDB.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/InvalidNodeIDException.h"
#include "exceptions/InvalidSchainException.h"
#include "headers/BlockFinalizeResponseHeader.h"
#include "headers/Header.h"
#include "monitoring/LivelinessMonitor.h"
#include "node/Node.h"
#include "node/NodeInfo.h"

BlockFinalizeResponder::BlockFinalizeResponder( Schain& _sChain ) : sChain( _sChain ) {}

BlockFinalizeRequestData BlockFinalizeResponder::parseRequest( const nlohmann::json& _jsonRequest ) {
    BlockFinalizeRequestData request;

    auto jsonRequest = _jsonRequest;
    request.schainID = Header::getUint64( jsonRequest, "schainID" );
    request.blockID = Header::getUint64( jsonRequest, "blockID" );
    request.proposerIndex = Header::getUint64( jsonRequest, "proposerIndex" );
    request.fragmentIndex = Header::getUint64( jsonRequest, "fragmentIndex" );
    request.nodeID = Header::getUint64( jsonRequest, "nodeID" );
#ifdef BITE
    request.epochID = Header::getUint64( jsonRequest, "epochID" );
    request.needDAProofSig = Header::getBool( jsonRequest, "needDASig" );
    request.needDecryptionShares = Header::getBool( jsonRequest, "needShares" );
    request.needFragmentData = Header::getBool( jsonRequest, "needData" );
#endif

    return request;
}

ptr< vector< uint8_t > > BlockFinalizeResponder::createResponse(
    const BlockFinalizeRequestData& _request,
    const ptr< BlockFinalizeResponseHeader >& _responseHeader ) {
    CHECK_ARGUMENT( _responseHeader );

    try {
        if ( ( uint64_t ) sChain.getSchainID() != _request.schainID ) {
            _responseHeader->setStatusSubStatus(
                CONNECTION_ERROR, CONNECTION_ERROR_UNKNOWN_SCHAIN_ID );
            BOOST_THROW_EXCEPTION( InvalidSchainException(
                "Incorrect schain " + to_string( _request.schainID ), __CLASS_NAME__ ) );
        }

        auto nmi = sChain.getNode()->getNodeInfoById( _request.nodeID );
        if ( nmi == nullptr ) {
            _responseHeader->setStatusSubStatus(
                CONNECTION_ERROR, CONNECTION_ERROR_DONT_KNOW_THIS_NODE );
            BOOST_THROW_EXCEPTION( InvalidNodeIDException(
                "Could not find node info for NODE_ID:" + to_string( ( uint64_t ) _request.nodeID ),
                __CLASS_NAME__ ) );
        }

        if ( _request.fragmentIndex < 1 ||
             ( uint64_t ) _request.fragmentIndex > sChain.getNodeCount() - 1 ) {
            CONS_LOG( debug, "Incorrect fragment index:" << to_string( _request.fragmentIndex ) );
            _responseHeader->setStatusSubStatus(
                CONNECTION_DISCONNECT, CONNECTION_ERROR_INVALID_FRAGMENT_INDEX );
            _responseHeader->setComplete();
            return nullptr;
        }

        if ( _request.proposerIndex < 1 ||
             ( uint64_t ) _request.proposerIndex > sChain.getNodeCount() ) {
            CONS_LOG( debug, "Incorrect proposer index:" << to_string( _request.proposerIndex ) );
            _responseHeader->setStatusSubStatus(
                CONNECTION_DISCONNECT, CONNECTION_ERROR_INVALID_PROPOSER_INDEX );
            _responseHeader->setComplete();
            return nullptr;
        }

        auto proposal = sChain.getNode()->getBlockProposalDB()->getBlockProposal(
            _request.blockID, _request.proposerIndex );
        string daSig;

        if ( !proposal || !sChain.getNode()->getDaProofDB()->haveDAProof( proposal ) ) {
            auto committedBlock = sChain.getBlock( _request.blockID );

            if ( committedBlock ) {
                if ( committedBlock->getProposerIndex() != ( uint64_t ) _request.proposerIndex ) {
                    _responseHeader->setStatusSubStatus( CONNECTION_DISCONNECT,
                        CONNECTION_FINALIZER_CLIENT_ASKING_FOR_INCORRECT_PROPOSER_INDEX );
                    CONS_LOG( err,
                        "Client asked for proposal with incorrect proposer index:" +
                            to_string( _request.proposerIndex ) +
                            ":committed block proposer index:" +
                            to_string( committedBlock->getProposerIndex() ) );
                    _responseHeader->setComplete();
                    return nullptr;
                }

                proposal = committedBlock;
                daSig = committedBlock->getDaSig();
            } else {
                _responseHeader->setStatusSubStatus(
                    CONNECTION_DISCONNECT, CONNECTION_FINALIZE_DONT_HAVE_PROPOSAL );
                _responseHeader->setComplete();
                return nullptr;
            }
        } else {
            daSig = sChain.getNode()->getDaProofDB()->getDASig(
                proposal->getBlockID(), proposal->getProposerIndex() );
        }

        CHECK_STATE2( !daSig.empty(), "Proposal has empty daSig" );

        auto hash = proposal->getHash().toHex();
        CHECK_STATE2( !hash.empty(), "Proposal has empty hash" );

#ifdef BITE
        ptr< AESKeyDecryptionShareList > myDecryptionShares;
        if ( _request.needDecryptionShares ) {
            myDecryptionShares = sChain.getNode()->getTEDecryptionDB()->getMyDecryptionShares(
                proposal->getBlockID(), proposal->getProposerIndex() );
            if ( !myDecryptionShares ) {
                _responseHeader->setStatusSubStatus(
                    CONNECTION_DISCONNECT, CONNECTION_FINALIZE_DONT_HAVE_DECRYPTION_SHARES );
                _responseHeader->setComplete();
                return nullptr;
            }
        } else {
            myDecryptionShares = make_shared< AESKeyDecryptionShareList >(
                _request.blockID, _request.proposerIndex, sChain.getSchainIndex() );
        }
#endif

        auto fragment = proposal->getFragment( ( uint64_t ) sChain.getNodeCount() - 1,
            _request.fragmentIndex
#ifdef BITE
            ,
            sChain.getSchainIndex(), myDecryptionShares
#endif
        );

        CHECK_STATE( fragment );

        _responseHeader->setStatusSubStatus( CONNECTION_PROCEED, CONNECTION_OK );

        auto serializedFragment = fragment->serialize(
#ifdef BITE
            _request.needDecryptionShares, _request.needFragmentData
#endif
        );
        CHECK_STATE( serializedFragment );

#ifdef BITE
        if ( !_request.needDAProofSig ) {
            daSig = "";
        }
#endif

        _responseHeader->setFragmentParams(
            serializedFragment->size(), proposal->serializeProposal()->size(), hash, daSig );

        return serializedFragment;
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __PRETTY_FUNCTION__, __CLASS_NAME__ ) );
    }
}
