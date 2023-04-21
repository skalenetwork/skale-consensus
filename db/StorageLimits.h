/*
    Copyright (C) 2019 -  SKALE Labs

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

    @file StorageLimits.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_STORAGELIMITS_H
#define SKALED_STORAGELIMITS_H


class StorageLimits {
    uint64_t storageUnitBytes = 0;
    uint64_t BLOCK_DB_SIZE = 0;
    uint64_t RANDOM_DB_SIZE = 0;
    uint64_t PRICE_DB_SIZE = 0;
    uint64_t PROPOSAL_HASH_DB_SIZE = 0;
    uint64_t PROPOSAL_VECTOR_DB_SIZE = 0;

    uint64_t OUTGOING_MSG_DB_SIZE = 0;
    uint64_t INCOMING_MSG_DB_SIZE = 0;
    uint64_t CONSENSUS_STATE_DB_SIZE = 0;
    uint64_t BLOCK_SIG_SHARE_DB_SIZE = 0;
    uint64_t DA_SIG_SHARE_DB_SIZE = 0;
    uint64_t DA_PROOF_DB_SIZE = 0;
    uint64_t BLOCK_PROPOSAL_DB_SIZE = 0;
    uint64_t INTERNAL_INFO_DB_SIZE = 0;


public:
    uint64_t getStorageUnitBytes() const;
    uint64_t getBlockDbSize() const;
    uint64_t getRandomDbSize() const;
    uint64_t getPriceDbSize() const;
    uint64_t getProposalHashDbSize() const;
    uint64_t getProposalVectorDbSize() const;
    uint64_t getOutgoingMsgDbSize() const;
    uint64_t getIncomingMsgDbSize() const;
    uint64_t getConsensusStateDbSize() const;
    uint64_t getBlockSigShareDbSize() const;
    uint64_t getDaSigShareDbSize() const;
    uint64_t getDaProofDbSize() const;
    uint64_t getBlockProposalDbSize() const;
    uint64_t getInternalInfoDbSize() const;


public:
    StorageLimits( uint64_t _totalStorageLimitBytes );
};


#endif  // SKALED_STORAGELIMITS_H
