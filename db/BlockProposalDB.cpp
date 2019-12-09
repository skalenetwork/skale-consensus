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

    @file ReceivedBlockProposalsDatabase.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
#include "../Agent.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/ExitRequestedException.h"
#include "../thirdparty/json.hpp"
#include "../abstracttcpserver/ConnectionStatus.h"
#include "leveldb/db.h"
#include "../node/Node.h"
#include "../chains/Schain.h"
#include "../crypto/SHAHash.h"
#include "../datastructures/DAProof.h"
#include "../datastructures/CommittedBlock.h"
#include "../pendingqueue/PendingTransactionsAgent.h"
#include "../blockproposal/pusher/BlockProposalClientAgent.h"
#include "../datastructures/BlockProposal.h"
#include "../datastructures/BlockProposalSet.h"


#include "BlockProposalDB.h"


using namespace std;


BlockProposalDB::BlockProposalDB(string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize,
                                 Schain &_sChain) :
                                 CacheLevelDB(_dirName, _prefix, _nodeId, _maxDBSize, _sChain.getTotalSigners(),
                                         _sChain.getRequiredSigners()) {

    sChain = &_sChain;
    try {
        oldBlockID = _sChain.getBootstrapBlockID();
    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }
};

void BlockProposalDB::addBlockProposal(ptr<BlockProposal> _proposal) {

    ASSERT(_proposal);
    CHECK_ARGUMENT(_proposal->getSignature() != nullptr);


    LOG(trace, "addBlockProposal blockID_=" + to_string(_proposal->getBlockID()) + " proposerIndex=" +
               to_string(_proposal->getProposerIndex()));


    auto serialized = _proposal->serialize();

    this->writeByteArrayToSet((const char*) serialized->data(), serialized->size(), _proposal->getBlockID(),
    _proposal->getProposerIndex());

}



ptr<vector<uint8_t> >
BlockProposalDB::getSerializedProposalFromLevelDB(block_id _blockID, schain_index _proposerIndex) {

    try {

        auto value = readStringFromBlockSet(_blockID, _proposerIndex);

        if (value) {
            auto serializedBlock = make_shared<vector<uint8_t>>();
            serializedBlock->insert(serializedBlock->begin(), value->data(), value->data() + value->size());
            CommittedBlock::serializedSanityCheck(serializedBlock);
            return serializedBlock;
        } else {
            return nullptr;
        }
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}




ptr<BlockProposal> BlockProposalDB::getBlockProposal(block_id _blockID, schain_index _proposerIndex) {


    LOCK(proposalMutex);


    auto serializedProposal = getSerializedProposalFromLevelDB(_blockID, _proposerIndex);

    if (serializedProposal == nullptr)
        return nullptr;

    auto proposal = BlockProposal::deserialize(serializedProposal, sChain->getCryptoManager());

    if (proposal == nullptr)
        return nullptr;

    CHECK_STATE(proposal->getSignature() != nullptr);

    return proposal;
}


const string BlockProposalDB::getFormatVersion() {
    return "1.0";
}

bool BlockProposalDB::proposalExists(block_id _blockId, schain_index _index) {
    return keyExistsInSet(_blockId, _index);
}
