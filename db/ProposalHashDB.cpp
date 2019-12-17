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

    @file ProposalHashDB.cpp
    @author Stan Kladko
    @date 2019
*/


#include "SkaleCommon.h"
#include "Log.h"
#include "crypto/SHAHash.h"
#include "chains/Schain.h"
#include "exceptions/InvalidStateException.h"
#include "datastructures/CommittedBlock.h"

#include "ProposalHashDB.h"
#include "CacheLevelDB.h"


ProposalHashDB::ProposalHashDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize)
        : CacheLevelDB(_sChain, _dirName, _prefix,
                       _nodeId, _maxDBSize, false) {
}


bool
ProposalHashDB::checkAndSaveHash(block_id _proposalBlockID, schain_index _proposerIndex, ptr<string> _proposalHash) {


    lock_guard<recursive_mutex> lock(m);

    try {

        auto key = createKey(_proposalBlockID, _proposerIndex);

        auto previous = readString(*key);

        if (previous == nullptr) {
            writeString(*key, *_proposalHash);
            return true;
        }

        return (*previous == *_proposalHash);

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

bool
ProposalHashDB::haveProposal(block_id _proposalBlockID, schain_index _proposerIndex) {


    lock_guard<recursive_mutex> lock(m);

    try {

        auto key = createKey(_proposalBlockID, _proposerIndex);

        auto previous = readString(*key);

        return (previous != nullptr);

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

const string ProposalHashDB::getFormatVersion() {
    return "1.0";
}





