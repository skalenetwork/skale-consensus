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

#include "Agent.h"
#include "SkaleCommon.h"
#include "Log.h"
#include "abstracttcpserver/ConnectionStatus.h"
#include "blockproposal/pusher/BlockProposalClientAgent.h"
#include "chains/Schain.h"
#include "crypto/BLAKE3Hash.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/BlockProposalSet.h"
#include "datastructures/CommittedBlock.h"
#include "datastructures/DAProof.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/FatalError.h"
#include "leveldb/db.h"
#include "monitoring/LivelinessMonitor.h"
#include "node/Node.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "thirdparty/json.hpp"


#include "BlockProposalDB.h"


using namespace std;

#define PROPOSAL_CACHE_SIZE 3

BlockProposalDB::BlockProposalDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId,
                                 uint64_t _maxDBSize) :
        CacheLevelDB(_sChain, _dirName, _prefix, _nodeId, _maxDBSize, true) {
    proposalCaches = make_shared<vector<ptr<cache::lru_cache<string, ptr<BlockProposal>>>>>();

    for (int i = 0; i < _sChain->getNodeCount(); i++) {

        auto emptyCache = make_shared<cache::lru_cache<string, ptr<BlockProposal>>>(PROPOSAL_CACHE_SIZE);

        proposalCaches->push_back(emptyCache);

    }



};

void BlockProposalDB::addBlockProposal(const ptr<BlockProposal>& _proposal) {

    MONITOR(__CLASS_NAME__, __FUNCTION__);

    CHECK_ARGUMENT(_proposal);
    CHECK_ARGUMENT(_proposal->getSignature() != "");

    auto proposerIndex =  _proposal->getProposerIndex();



    CHECK_STATE((uint64_t) proposerIndex <= getSchain()->getNodeCount() );

    LOG(trace, "addBlockProposal blockID_=" + to_string(_proposal->getBlockID()) + " proposerIndex=" +
               to_string(_proposal->getProposerIndex()));

    auto key = createKey(_proposal->getBlockID(), _proposal->getProposerIndex());
    CHECK_STATE(key != "");


    auto cache = proposalCaches->at((uint64_t) proposerIndex - 1);
    CHECK_STATE(cache);
    cache->putIfDoesNotExist(key, _proposal);

    // dont save non-own proposals
    if (_proposal->getProposerIndex() !=  getSchain()->getSchainIndex())
        return;

    try {

        ptr<vector<uint8_t> > serialized;

        serialized = _proposal->serialize();
        CHECK_STATE(serialized);

        this->writeByteArrayToSet((const char *) serialized->data(), serialized->size(), _proposal->getBlockID(),
                                  _proposal->getProposerIndex());

    } catch (ExitRequestedException &e) { throw; }
    catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


ptr<vector<uint8_t> >
BlockProposalDB::getMyProposalFromLevelDB(block_id _blockID, schain_index _proposerIndex) {

    try {

        auto value = readStringFromBlockSet(_blockID, _proposerIndex);

        if (value != "") {
            auto serializedBlock = make_shared<vector<uint8_t>>();
            serializedBlock->insert(serializedBlock->begin(), value.data(), value.data() + value.size());
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

    MONITOR( __CLASS_NAME__, __FUNCTION__)

    auto key = createKey(_blockID, _proposerIndex);
    CHECK_STATE(!key.empty());

    auto cache = proposalCaches->at((uint64_t) _proposerIndex - 1);

    CHECK_STATE(cache);

    if (auto result = cache->getIfExists(key); result.has_value()) {
            return any_cast<ptr<BlockProposal>>(result);
    }

    if (getSchain()->getSchainIndex() != _proposerIndex) {
        // non-owned proposals are never saved in DB
        return nullptr;
    }

    auto serializedProposal = getMyProposalFromLevelDB( _blockID, _proposerIndex );

    if (serializedProposal == nullptr)
        return nullptr;


    // dont check signatures on proposals stored in the db since they have already been verified
    auto proposal = BlockProposal::deserialize(serializedProposal, getSchain()->getCryptoManager(), false);

    if (proposal == nullptr)
        return nullptr;


    CHECK_STATE( proposalCaches )

    cache->putIfDoesNotExist(key, proposal);

    CHECK_STATE(!proposal->getSignature().empty());

    return proposal;
}


const string& BlockProposalDB::getFormatVersion() {
    static const string version = "1.0";
    return version;
}

bool BlockProposalDB::proposalExists(block_id _blockId, schain_index _index) {
    return keyExistsInSet(_blockId, _index);
}


