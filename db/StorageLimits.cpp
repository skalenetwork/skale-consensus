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

StorageLimits::StorageLimits( uint64_t _totalStorageLimitBytes )
    : storageUnitBytes( _totalStorageLimitBytes ) {

    // Calculate the total denominator based on the enabled features
    uint32_t totalFractions = (StorageConfig::FRACTION_LARGE) 
                            + (9 * StorageConfig::FRACTION_SMALL) 
                            + (StorageConfig::FRACTION_MEDIUM)
                            + (StorageConfig::FRACTION_TINY);

#ifdef BITE
    totalFractions += (StorageConfig::FRACTION_TINY + StorageConfig::FRACTION_MEDIUM);
#else
    totalFractions += StorageConfig::FRACTION_CUSTOM_INCOMING_MSG_DB_SIZE;
#endif


    // The "unit" is now literally the size of one "fraction" or "share"
    const auto fractionSize = _totalStorageLimitBytes / (LEVELDB_SHARDS * totalFractions);

    BLOCK_DB_SIZE = StorageConfig::FRACTION_LARGE * fractionSize;
    RANDOM_DB_SIZE = StorageConfig::FRACTION_SMALL * fractionSize;
    PRICE_DB_SIZE = StorageConfig::FRACTION_SMALL * fractionSize;
    PROPOSAL_HASH_DB_SIZE = StorageConfig::FRACTION_SMALL * fractionSize;
    PROPOSAL_VECTOR_DB_SIZE = StorageConfig::FRACTION_SMALL * fractionSize;
    OUTGOING_MSG_DB_SIZE = StorageConfig::FRACTION_SMALL * fractionSize;
    CONSENSUS_STATE_DB_SIZE = StorageConfig::FRACTION_SMALL * fractionSize;
    BLOCK_SIG_SHARE_DB_SIZE = StorageConfig::FRACTION_SMALL * fractionSize;
    DA_SIG_SHARE_DB_SIZE = StorageConfig::FRACTION_SMALL * fractionSize;
    DA_PROOF_DB_SIZE = StorageConfig::FRACTION_SMALL * fractionSize;
    BLOCK_PROPOSAL_DB_SIZE = StorageConfig::FRACTION_MEDIUM * fractionSize;
    INTERNAL_INFO_DB_SIZE = StorageConfig::FRACTION_TINY * fractionSize;
#ifdef BITE
    // incoming message db is not used anymore
    INCOMING_MSG_DB_SIZE = StorageConfig::FRACTION_TINY * fractionSize;
    TE_DECRYPTION_DB_SIZE = StorageConfig::FRACTION_MEDIUM * fractionSize;
#else
    INCOMING_MSG_DB_SIZE = StorageConfig::FRACTION_CUSTOM_INCOMING_MSG_DB_SIZE * fractionSize;
#endif
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

#ifdef BITE
uint64_t StorageLimits::getTEDecryptionDbSize() const {
    return TE_DECRYPTION_DB_SIZE;
}
#endif
