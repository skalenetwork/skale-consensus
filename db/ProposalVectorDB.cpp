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

    @file ProposalVectorDB.cpp
    @author Stan Kladko
    @date 2019
*/


#include "SkaleCommon.h"
#include "Log.h"
#include "chains/Schain.h"
#include "datastructures/BooleanProposalVector.h"
#include "exceptions/InvalidStateException.h"

#include "ProposalVectorDB.h"
#include "CacheLevelDB.h"


ProposalVectorDB::ProposalVectorDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize)
        : CacheLevelDB(_sChain, _dirName, _prefix,
                       _nodeId, _maxDBSize, false) {
}



// Proposal vector may already be saved in DB and consensus may already be started
// Then do not save
// Return if it was saved.
bool
ProposalVectorDB::trySavingProposalVector(block_id _proposalBlockID, const ptr<BooleanProposalVector>& _proposalVector) {

    CHECK_ARGUMENT(_proposalVector)

    lock_guard<recursive_mutex> lock(m);

    auto proposalString = _proposalVector->toString();

    CHECK_STATE(!proposalString.empty())

    try {

        auto key = createKey(_proposalBlockID);
        CHECK_STATE(!key.empty())

        auto previous = readString(key);

        if (previous.empty()) {
            writeString(key, proposalString);
            return true;
        } else {
            return false;
        }

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

ptr<BooleanProposalVector>
ProposalVectorDB::getVector(block_id _blockID) {

    lock_guard<recursive_mutex> lock(m);

    try {

        auto key = createKey(_blockID);
        CHECK_STATE(!key.empty())

        auto value = readString(key);

        if (value.empty()) {
            return nullptr;
        }
        return make_shared<BooleanProposalVector>(getSchain()->getNodeCount(), value);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

const string& ProposalVectorDB::getFormatVersion() {
    static const string version = "1.0";
    return version;
}





