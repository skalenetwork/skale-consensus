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

    @file CommittedBlockHeader.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once

#include "Header.h"

class NodeInfo;
class BlockProposal;
class Schain;
class CommittedBlock;

class BlockProposalHeader : public Header {


    schain_id schainID;
    schain_index proposerIndex;
    node_id proposerNodeID;
    block_id blockID;
    ptr<string> blockHash;
    ptr<string> signature;
    ptr<vector<uint64_t>> transactionSizes;
    uint64_t timeStamp = 0;
    uint32_t timeStampMs = 0;
public:
    const u256 &getStateRoot() const;

private:
    u256 stateRoot = 0;

public:

    const ptr<string> &getSignature() const;

    const schain_id &getSchainID() const;

    const block_id &getBlockID() const;


    BlockProposalHeader(nlohmann::json& _json);

    BlockProposalHeader(BlockProposal & _block);

    ptr<string> getBlockHash() const {
        return blockHash;
    }

    void addFields(nlohmann::json &j) override;

    const ptr<vector<uint64_t>> &getTransactionSizes() const;


    const schain_index &getProposerIndex() const;

    const node_id &getProposerNodeId() const;


    uint64_t getTimeStamp() const;

    uint32_t getTimeStampMs() const;


};



