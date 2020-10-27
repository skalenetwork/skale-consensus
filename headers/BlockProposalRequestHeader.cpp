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

    @file BlockProposalHeader.cpp
    @author Stan Kladko
    @date 2018
*/

#include "Log.h"
#include "SkaleCommon.h"
#include "crypto/SHAHash.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"

#include "chains/Schain.h"
#include "datastructures/BlockProposal.h"
#include "node/NodeInfo.h"

#include "AbstractBlockRequestHeader.h"

#include "BlockProposalRequestHeader.h"

using namespace std;

BlockProposalRequestHeader::BlockProposalRequestHeader(
    rapidjson::Document& _proposalRequest, node_count _nodeCount )
    : AbstractBlockRequestHeader( _nodeCount,
          ( schain_id ) Header::getUint64Rapid( _proposalRequest, "schainID" ),
          ( block_id ) Header::getUint64Rapid( _proposalRequest, "blockID" ), Header::BLOCK_PROPOSAL_REQ,
          ( schain_index ) Header::getUint64Rapid( _proposalRequest, "proposerIndex" ) ) {
    proposerNodeID = ( node_id ) Header::getUint64Rapid( _proposalRequest, "proposerNodeID" );
    timeStamp = Header::getUint64Rapid( _proposalRequest, "timeStamp" );
    timeStampMs = Header::getUint32Rapid( _proposalRequest, "timeStampMs" );
    hash = Header::getStringRapid( _proposalRequest, "hash" );
    CHECK_STATE( !hash.empty() )
    signature = Header::getStringRapid( _proposalRequest, "sig" );
    CHECK_STATE( !signature.empty() )
    txCount = Header::getUint64Rapid( _proposalRequest, "txCount" );
    auto stateRootStr = Header::getStringRapid( _proposalRequest, "sr" );
    CHECK_STATE( !stateRootStr.empty() )
    stateRoot = u256( stateRootStr );
    CHECK_STATE( stateRoot != 0 )
}

BlockProposalRequestHeader::BlockProposalRequestHeader(
    Schain& _sChain, const ptr< BlockProposal >& proposal )
    : AbstractBlockRequestHeader( _sChain.getNodeCount(), _sChain.getSchainID(),
          proposal->getBlockID(), Header::BLOCK_PROPOSAL_REQ, _sChain.getSchainIndex() ) {
    proposerNodeID = _sChain.getNode()->getNodeID();
    txCount = ( uint64_t ) proposal->getTransactionCount();
    timeStamp = proposal->getTimeStamp();
    timeStampMs = proposal->getTimeStampMs();

    hash = proposal->getHash()->toHex();
    CHECK_STATE( !hash.empty() )

    signature = proposal->getSignature();

    stateRoot = proposal->getStateRoot();

    CHECK_STATE( stateRoot != 0 )
    CHECK_STATE( timeStamp > MODERN_TIME )

    complete = true;
}


void BlockProposalRequestHeader::addFields(
    rapidjson::Writer< rapidjson::StringBuffer >& jsonRequest ) {
    AbstractBlockRequestHeader::addFields( jsonRequest );

    jsonRequest.String( "schainID" );
    jsonRequest.Uint64( ( uint64_t ) schainID );

    jsonRequest.String( "proposerNodeID" );
    jsonRequest.Uint64( ( uint64_t ) proposerNodeID );

    jsonRequest.String( "proposerIndex" );
    jsonRequest.Uint64( ( uint64_t ) proposerIndex );

    jsonRequest.String( "blockID" );
    jsonRequest.Uint64( ( uint64_t ) blockID );

    jsonRequest.String( "txCount" );
    jsonRequest.Uint64( txCount );

    CHECK_STATE( timeStamp > MODERN_TIME )
    jsonRequest.String( "timeStamp" );
    jsonRequest.Uint64( timeStamp );

    jsonRequest.String( "timeStampMs" );
    jsonRequest.Uint( timeStampMs );

    CHECK_STATE( !hash.empty() )
    CHECK_STATE( !signature.empty() )

    jsonRequest.String( "hash" );
    jsonRequest.String( hash.c_str() );

    jsonRequest.String( "sig" );
    jsonRequest.String( signature.c_str() );

    jsonRequest.String( "sr" );
    jsonRequest.String( stateRoot.str().c_str() );
}
node_id BlockProposalRequestHeader::getProposerNodeId() {
    return proposerNodeID;
}

string BlockProposalRequestHeader::getHash() {
    CHECK_STATE( !hash.empty() )
    return hash;
}

uint64_t BlockProposalRequestHeader::getTxCount() const {
    return txCount;
}

uint64_t BlockProposalRequestHeader::getTimeStamp() const {
    return timeStamp;
}

uint32_t BlockProposalRequestHeader::getTimeStampMs() const {
    return timeStampMs;
}

string BlockProposalRequestHeader::getSignature() {
    CHECK_STATE( !signature.empty() )
    return signature;
}

u256 BlockProposalRequestHeader::getStateRoot() {
    return stateRoot;
}
