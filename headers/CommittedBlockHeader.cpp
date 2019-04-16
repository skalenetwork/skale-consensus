//
// Created by stan on 18.03.18.
//

#include "../SkaleConfig.h"
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

CommittedBlockHeader::CommittedBlockHeader() {

}


CommittedBlockHeader::CommittedBlockHeader(CommittedBlock& _block) {

    this->proposerIndex = _block.getProposerIndex();
    this->proposerNodeID = _block.getProposerNodeID();
    this->schainID = _block.getSchainID();
    this->blockID = _block.getBlockID();
    this->blockHash = _block.getHash();
    this->timeStamp = _block.getTimeStamp();

    auto items = _block.getTransactionList()->getItems();

    for (auto && t : *items) {
        transactionSizes.push_back(t->getData()->size());
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

    j["sizes"] = transactionSizes;

    j["timeStamp"] = timeStamp;

    ASSERT(timeStamp > 0);



}


