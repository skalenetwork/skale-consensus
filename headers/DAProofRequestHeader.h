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

    @file DAProofHeader.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once

#include "AbstractBlockRequestHeader.h"

class DAProof;


class DAProofRequestHeader : public AbstractBlockRequestHeader{



    node_id proposerNodeID;
    ptr<string> thresholdSig;
    ptr<string> blockHash;


public:

    DAProofRequestHeader(Schain &_sChain, ptr<DAProof> _proof);

    DAProofRequestHeader(nlohmann::json _proposalRequest, node_count _nodeCount);

    void addFields(nlohmann::basic_json<> &jsonRequest) override;

    const node_id &getProposerNodeId() const;

    ptr<string> getSignature() const;

    ptr<string> getBlockHash() const;

};



