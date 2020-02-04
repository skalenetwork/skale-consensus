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

    @file ReceivedBlockProposal.h
    @author Stan Kladko
    @date 2018
*/

#pragma once


#include "BlockProposal.h"



class ReceivedBlockProposal : public BlockProposal{
public:

    ReceivedBlockProposal(Schain &_sChain, const block_id &_blockID, const uint64_t &_timeStamp,
                          const uint32_t &_timeStampMs);


    ReceivedBlockProposal(Schain &_sChain, const block_id &_blockID, const schain_index &_proposerIndex,
                          const ptr<TransactionList> &_transactions, u256 _stateRoot, const uint64_t &_timeStamp,
                          const uint32_t &_timeStampMs, ptr<string> _hash, ptr<string> _signature);


    static int64_t getTotalObjects() {
        return totalObjects;
    }

    virtual ~ReceivedBlockProposal();

private:

    static atomic<int64_t>  totalObjects;

};
