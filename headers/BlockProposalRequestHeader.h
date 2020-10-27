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

    @file BlockProposalHeader.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once

#include "AbstractBlockRequestHeader.h"



class BlockProposalRequestHeader : public AbstractBlockRequestHeader{



    node_id proposerNodeID{};
    string hash;
    string signature;


    uint64_t txCount;
    uint64_t  timeStamp = 0;
    uint32_t  timeStampMs = 0;
    u256 stateRoot;

public:

    BlockProposalRequestHeader(Schain &_sChain, const ptr<BlockProposal>& proposal);

    BlockProposalRequestHeader(rapidjson::Document& _proposalRequest, node_count _nodeCount);


    void addFields(rapidjson::Writer<rapidjson::StringBuffer> &jsonRequest) override;

    node_id getProposerNodeId();

    string getHash();

    uint64_t getTxCount() const;

    uint64_t getTimeStamp() const;

    uint32_t getTimeStampMs() const;

    string getSignature();

    u256 getStateRoot();

};



