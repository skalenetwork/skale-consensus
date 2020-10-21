/*
    Copyright (C) 2019 SKALE Labs

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

    @file ProposalVectorDB.h
    @author Stan Kladko
    @date 2019
*/


#ifndef SKALED_PROPOSAL_VECTOR_DB_H
#define SKALED_PROPOSAL_VECTOR_DB_


#include "CacheLevelDB.h"

class CryptoManager;

class ProposalVectorDB : public CacheLevelDB {

    recursive_mutex m;

public:

    ProposalVectorDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize);

    bool trySavingProposalVector(block_id _proposalBlockID, const ptr<BooleanProposalVector>& _proposalVector);

    ptr<BooleanProposalVector> getVector(block_id _blockID);

    const string& getFormatVersion() override ;
};


#endif //SKALED_PROPOSAL_VECTOR_DB_H
