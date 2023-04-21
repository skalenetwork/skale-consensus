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

#pragma once

#include "Header.h"

class NodeInfo;
class BlockProposal;
class Schain;
class CommittedBlock;

class BlockProposalHeader : public BasicHeader {
    schain_id schainID{};
    schain_index proposerIndex{};
    node_id proposerNodeID{};
    block_id blockID{};
    string blockHash;
    string signature;
    ptr< vector< uint64_t > > transactionSizes;
    uint64_t timeStamp = 0;
    uint32_t timeStampMs = 0;
    u256 stateRoot = 0;

public:
    u256 getStateRoot();

    string getSignature();

    schain_id getSchainID();

    block_id getBlockID();

    explicit BlockProposalHeader( nlohmann::json& _json );

    explicit BlockProposalHeader( BlockProposal& _block );

    string getBlockHash() {
        CHECK_STATE( !blockHash.empty() );
        return blockHash;
    }

    void addFields( nlohmann::json& j ) override;

    ptr< vector< uint64_t > > getTransactionSizes();

    schain_index getProposerIndex();

    node_id getProposerNodeId();

    [[nodiscard]] uint64_t getTimeStamp() const;

    [[nodiscard]] uint32_t getTimeStampMs() const;
};
