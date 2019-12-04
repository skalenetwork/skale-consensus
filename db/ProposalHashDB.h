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

    @file ProposalHashDB.h
    @author Stan Kladko
    @date 2019
*/


#ifndef SKALED_ProposalHashDB_H
#define SKALED_ProposalHashDB_H

class CommittedBlock;



#include "LevelDB.h"

class CryptoManager;

class ProposalHashDB : public LevelDB {

    recursive_mutex m;

public:

    ProposalHashDB(string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize);

    bool checkAndSaveHash(block_id _proposalBlockID, schain_index _proposerIndex, ptr<string> _proposalHash);

    bool haveProposal(block_id _proposalBlockID, schain_index _proposerIndex);

    const string getFormatVersion() override ;
};


#endif //SKALED_ProposalHashDB_H
