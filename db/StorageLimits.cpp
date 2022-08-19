/*
    Copyright (C) 2020 SKALE Labs

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

    @file StorageLimits.cpp
    @author Stan Kladko
    @date 2020
*/
#include "SkaleCommon.h"
#include "Log.h"
#include "StorageLimits.h"

StorageLimits::StorageLimits( uint64_t _totalStorageLimitBytes ) : storageUnitBytes( _totalStorageLimitBytes ) {

    auto unit = _totalStorageLimitBytes / (LEVELDB_SHARDS *(1000 + 10 * 10 + 100));

    BLOCK_DB_SIZE = 1000 * unit;
    RANDOM_DB_SIZE = 10 * unit;
    PRICE_DB_SIZE = 10 *  unit;
    PROPOSAL_HASH_DB_SIZE = 10 * unit;
    PROPOSAL_VECTOR_DB_SIZE = 10  * unit;
    OUTGOING_MSG_DB_SIZE = 10 * unit;
    CONSENSUS_STATE_DB_SIZE = 10 * unit;
    BLOCK_SIG_SHARE_DB_SIZE = 10 * unit;
    DA_SIG_SHARE_DB_SIZE = 10 * unit;
    DA_PROOF_DB_SIZE = 10 * unit;
    BLOCK_PROPOSAL_DB_SIZE = 100 * unit;
    INTERNAL_INFO_DB_SIZE = 1 * unit;
    INCOMING_MSG_DB_SIZE = 9 * unit;
}
uint64_t StorageLimits::getStorageUnitBytes() const {
    return storageUnitBytes;
}
uint64_t StorageLimits::getBlockDbSize() const {
    return BLOCK_DB_SIZE;
}
uint64_t StorageLimits::getRandomDbSize() const {
    return RANDOM_DB_SIZE;
}
uint64_t StorageLimits::getPriceDbSize() const {
    return PRICE_DB_SIZE;
}
uint64_t StorageLimits::getProposalHashDbSize() const {
    return PROPOSAL_HASH_DB_SIZE;
}
uint64_t StorageLimits::getProposalVectorDbSize() const {
    return PROPOSAL_VECTOR_DB_SIZE;
}
uint64_t StorageLimits::getOutgoingMsgDbSize() const {
    return OUTGOING_MSG_DB_SIZE;
}
uint64_t StorageLimits::getIncomingMsgDbSize() const {
    return INCOMING_MSG_DB_SIZE;
}
uint64_t StorageLimits::getConsensusStateDbSize() const {
    return CONSENSUS_STATE_DB_SIZE;
}
uint64_t StorageLimits::getBlockSigShareDbSize() const {
    return BLOCK_SIG_SHARE_DB_SIZE;
}
uint64_t StorageLimits::getDaSigShareDbSize() const {
    return DA_SIG_SHARE_DB_SIZE;
}
uint64_t StorageLimits::getDaProofDbSize() const {
    return DA_PROOF_DB_SIZE;
}
uint64_t StorageLimits::getBlockProposalDbSize() const {
    return BLOCK_PROPOSAL_DB_SIZE;
}

uint64_t StorageLimits::getInternalInfoDbSize() const {
    return INTERNAL_INFO_DB_SIZE;
}