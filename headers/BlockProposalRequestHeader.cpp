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

#include "SkaleCommon.h"

#include "Log.h"

#include "crypto/BLAKE3Hash.h"

#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"

#include "datastructures/BlockProposal.h"
#include "node/NodeInfo.h"
#include "chains/Schain.h"

#include "AbstractBlockRequestHeader.h"

#include "BlockProposalRequestHeader.h"

using namespace std;

BlockProposalRequestHeader::BlockProposalRequestHeader(
    nlohmann::json _proposalRequest, node_count _nodeCount )
    : AbstractBlockRequestHeader( _nodeCount,
          ( schain_id ) Header::getUint64( _proposalRequest, "schainID" ),
          ( block_id ) Header::getUint64( _proposalRequest, "blockID" ), Header::BLOCK_PROPOSAL_REQ,
          ( schain_index ) Header::getUint64( _proposalRequest, "proposerIndex" ) ) {
    proposerNodeID = ( node_id ) Header::getUint64( _proposalRequest, "proposerNodeID" );
    timeStamp = Header::getUint64( _proposalRequest, "timeStamp" );
    timeStampMs = Header::getUint32( _proposalRequest, "timeStampMs" );
    hash = Header::getString( _proposalRequest, "hash" );
    CHECK_STATE( !hash.empty() )
    signature = Header::getString( _proposalRequest, "sig" );
    CHECK_STATE( !signature.empty() )
    txCount = Header::getUint64( _proposalRequest, "txCount" );
    auto stateRootStr = Header::getString( _proposalRequest, "sr" );
    CHECK_STATE( !stateRootStr.empty() )
    stateRoot = u256( stateRootStr );
}

BlockProposalRequestHeader::BlockProposalRequestHeader( Schain& _sChain, BlockProposal& _proposal )
    : AbstractBlockRequestHeader( _sChain.getNodeCount(), _sChain.getSchainID(),
          _proposal.getBlockID(), Header::BLOCK_PROPOSAL_REQ, _sChain.getSchainIndex() ) {
    proposerNodeID = _sChain.getNode()->getNodeID();
    txCount = ( uint64_t ) _proposal.getTransactionCount();
    timeStamp = _proposal.getTimeStampS();
    timeStampMs = _proposal.getTimeStampMs();

    hash = _proposal.getHash().toHex();
    CHECK_STATE( !hash.empty() )

    signature = _proposal.getSignature();

    stateRoot = _proposal.getStateRoot();

    CHECK_STATE( timeStamp > MODERN_TIME )

    complete = true;
}


void BlockProposalRequestHeader::addFields( nlohmann::basic_json<>& _jsonRequest ) {
    AbstractBlockRequestHeader::addFields( _jsonRequest );

    _jsonRequest["schainID"] = ( uint64_t ) schainID;
    _jsonRequest["proposerNodeID"] = ( uint64_t ) proposerNodeID;
    _jsonRequest["proposerIndex"] = ( uint64_t ) proposerIndex;
    _jsonRequest["blockID"] = ( uint64_t ) blockID;
    _jsonRequest["txCount"] = txCount;
    CHECK_STATE( timeStamp > MODERN_TIME )
    _jsonRequest["timeStamp"] = timeStamp;
    _jsonRequest["timeStampMs"] = timeStampMs;
    CHECK_STATE( !hash.empty() )
    CHECK_STATE( !signature.empty() )
    _jsonRequest["hash"] = hash;
    _jsonRequest["sig"] = signature;
    _jsonRequest["sr"] = stateRoot.str();
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
