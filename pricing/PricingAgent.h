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

    @file PricingAgent.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_PRICINGAGENT_H
#define SKALED_PRICINGAGENT_H

#include "../Agent.h"

class PricingStrategy;

class PricingAgent : public Agent{

    ptr<PricingStrategy> pricingStrategy;

public:

    explicit PricingAgent(Schain& _sChain);

    u256 calculatePrice(const ConsensusExtFace::transactions_vector &_approvedTransactions,
                                uint64_t _timeStamp, uint32_t  _timeStampMs, block_id _blockID);

    u256 readPrice(block_id _blockId);

    void savePrice(u256 price, block_id _blockID);




};


#endif //SKALED_PRICINGAGENT_H
