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

    @file CommittedBlockHeader.cpp
    @author Stan Kladko
    @date 2018
*/
#include "SkaleCommon.h"
#include "Log.h"

#include "crypto/SHAHash.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/CommittedBlock.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"
#include <network/Utils.h>
#include "chains/Schain.h"
#include "BlockProposalRequestHeader.h"
#include "BlockProposalHeader.h"



using namespace std;





BlockProposalHeader::BlockProposalHeader(BlockProposal& _block) : BasicHeader(Header::BLOCK) {

    this->proposerIndex = _block.getProposerIndex();
    this->proposerNodeID = _block.getProposerNodeID();
    this->schainID = _block.getSchainID();
    this->blockID = _block.getBlockID();
    this->blockHash = _block.getHash()->toHex();
    this->stateRoot = _block.getStateRoot();
    this->signature = _block.getSignature();
    this->timeStamp = _block.getTimeStamp();
    this->timeStampMs = _block.getTimeStampMs();
    this->transactionSizes = make_shared<vector<uint64_t>>();

    auto items = _block.getTransactionList()->getItems();
    CHECK_STATE(items)

    for (auto && t : *items) {
        transactionSizes->push_back(t->getSerializedSize(true));
    }
    setComplete();
}


schain_id BlockProposalHeader::getSchainID() {
    return schainID;
}


block_id BlockProposalHeader::getBlockID() {
    return blockID;
}

void BlockProposalHeader::addFields(nlohmann::json &j) {

    j["schainID"] = (uint64_t ) schainID;

    j["proposerIndex"] = (uint64_t ) proposerIndex;

    j["proposerNodeID"] = (uint64_t ) proposerNodeID;

    j["blockID"] = (uint64_t ) blockID;

    j["hash"] = blockHash;

    j["sig"] = signature;

    j["sizes"] = *transactionSizes;

    j["timeStamp"] = timeStamp;

    j["timeStampMs"] = timeStampMs;

    j["sr"] = stateRoot.str();

    CHECK_STATE(stateRoot != 0)

    ASSERT(timeStamp > 0)
}

BlockProposalHeader::BlockProposalHeader(nlohmann::json& _json) : BasicHeader(Header::BLOCK){

    proposerIndex = schain_index( Header::getUint64(_json, "proposerIndex" ) );
    proposerNodeID = node_id( Header::getUint64(_json, "proposerNodeID" ) );
    blockID = block_id( Header::getUint64(_json, "blockID" ) );
    schainID = schain_id( Header::getUint64(_json, "schainID" ) );
    timeStamp = Header::getUint64(_json, "timeStamp" );
    timeStampMs = Header::getUint32(_json, "timeStampMs" );
    blockHash = Header::getString(_json, "hash" ) ;
    signature = Header::getString(_json, "sig");
    auto srStr = Header::getString(_json, "sr");
    stateRoot = u256(srStr);
    CHECK_STATE(stateRoot != 0)

    Header::nullCheck(_json, "sizes" );
    nlohmann::json jsonTransactionSizes = _json["sizes"];

    transactionSizes = make_shared<vector< uint64_t > >();

    for ( auto&& jsize : jsonTransactionSizes ) {
        transactionSizes->push_back( jsize );
    }

    setComplete();
}

 ptr<vector<uint64_t>> BlockProposalHeader::getTransactionSizes()  {
    return transactionSizes;
}

string BlockProposalHeader::getSignature()  {
    CHECK_STATE(!signature.empty())
    return signature;
}

 schain_index BlockProposalHeader::getProposerIndex() {
    return proposerIndex;
}

 node_id BlockProposalHeader::getProposerNodeId() {
    return proposerNodeID;
}


uint64_t BlockProposalHeader::getTimeStamp() const {
    return timeStamp;
}

uint32_t BlockProposalHeader::getTimeStampMs() const  {
    return timeStampMs;
}

u256 BlockProposalHeader::getStateRoot()  {
    return stateRoot;
}


