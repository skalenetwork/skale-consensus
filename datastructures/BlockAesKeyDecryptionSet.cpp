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

    @file Set.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"

#include "node/ConsensusEngine.h"
#include "crypto/BLAKE3Hash.h"
#include "datastructures/BlockAesKeyDecryptionShare.h"
#include "datastructures/BooleanProposalVector.h"

#include "chains/Schain.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "datastructures/DAProof.h"

#include "BlockAesKeyDecryptionSet.h"

using namespace std;

bool BlockAesKeyDecryptionSet::add(const ptr<BlockAesKeyDecryptionShare>& _decryption) {

    CHECK_ARGUMENT( _decryption);

    auto index = (uint64_t ) _decryption->getSchainIndex();

    CHECK_STATE(index > 0 && index <= nodeCount)

    LOCK(m)

    if ( decryptions.count(index) > 0 ) {
        LOG(trace,
            "Got block decryption with the same index" + to_string(index));
        return false;
    }

    decryptions.emplace(index,_decryption);

    return true;
}



BlockAesKeyDecryptionSet::BlockAesKeyDecryptionSet(Schain* _sChain, block_id _blockId)
    : blockId(_blockId){
    CHECK_ARGUMENT(_sChain);
    CHECK_ARGUMENT(_blockId > 0);

    nodeCount = _sChain->getNodeCount();
    requiredDecryptionCount = _sChain->getRequiredSigners();
    totalObjects++;
}

BlockAesKeyDecryptionSet::~BlockAesKeyDecryptionSet() {
    totalObjects--;
}

node_count BlockAesKeyDecryptionSet::getCount() {
    LOCK(m)
    return ( node_count ) decryptions.size();
}


atomic<int64_t>  BlockAesKeyDecryptionSet::totalObjects(0);

bool BlockAesKeyDecryptionSet::isEnough()  {
    LOCK(m)
    return decryptions.size() >= this->requiredDecryptionCount;
}
