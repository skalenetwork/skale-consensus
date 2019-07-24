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

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../thirdparty/json.hpp"
#include "../crypto/SHAHash.h"
#include "../abstracttcpserver/ConnectionStatus.h"
#include "BlockProposalHeader.h"
#include "../datastructures/BlockProposal.h"
#include "../datastructures/CommittedBlock.h"
#include "../datastructures/Transaction.h"
#include "../datastructures/TransactionList.h"

#include "../chains/Schain.h"
#include "../headers/CommittedBlockHeader.h"


using namespace std;

CommittedBlockHeader::CommittedBlockHeader() : Header(Header::COMMITTED_BLOCK) {

}


CommittedBlockHeader::CommittedBlockHeader(CommittedBlock& _block) : Header(Header::COMMITTED_BLOCK) {

    this->proposerIndex = _block.getProposerIndex();
    this->proposerNodeID = _block.getProposerNodeID();
    this->schainID = _block.getSchainID();
    this->blockID = _block.getBlockID();
    this->blockHash = _block.getHash();
    this->timeStamp = _block.getTimeStamp();
    this->timeStampMs = _block.getTimeStampMs();
    this->transactionSizes = make_shared<list<uint32_t>>();

    auto items = _block.getTransactionList()->getItems();

    for (auto && t : *items) {
        transactionSizes->push_back(t->getSerializedSize(true));
    }
    setComplete();
}


const schain_id &CommittedBlockHeader::getSchainID() const {
    return schainID;
}


const block_id &CommittedBlockHeader::getBlockID() const {
    return blockID;
}

void CommittedBlockHeader::addFields(nlohmann::basic_json<> &j) {

    auto hash = getBlockHash();

    j["schainID"] = (uint64_t ) schainID;

    j["proposerIndex"] = (uint64_t ) proposerIndex;

    j["proposerNodeID"] = (uint64_t ) proposerNodeID;

    j["blockID"] = (uint64_t ) blockID;

    j["hash"] = *(hash->toHex());

    j["sizes"] = *transactionSizes;

    j["timeStamp"] = timeStamp;

    j["timeStampMs"] = timeStampMs;

    ASSERT(timeStamp > 0);



}


