/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file ReceivedBlockProposal.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
#include "../crypto/SHAHash.h"
#include "ReceivedBlockProposal.h"

ReceivedBlockProposal::ReceivedBlockProposal(Schain &_sChain, const block_id &_blockID,
                                             const schain_index &_proposerIndex,
                                             const ptr<TransactionList> &_transactions,
                                             const uint64_t &_timeStamp,
                                             const uint32_t &_timeStampMs) : BlockProposal(_sChain, _blockID,
                                                                                         _proposerIndex, _transactions,
                                                                                         _timeStamp, _timeStampMs) {
    totalObjects++;
}




atomic<uint64_t>  ReceivedBlockProposal::totalObjects(0);

ReceivedBlockProposal::~ReceivedBlockProposal() {
    totalObjects--;
}


