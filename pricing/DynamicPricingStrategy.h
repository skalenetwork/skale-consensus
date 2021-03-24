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

    @file DynamicPricingStrategy.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_DYNAMICPRICINGSTRATEGY_H
#define SKALED_DYNAMICPRICINGSTRATEGY_H

#include "PricingStrategy.h"

class DynamicPricingStrategy : public PricingStrategy {

    u256 minPrice = 100000;
    u256 maxPrice = pow(u256(2), 200);
    uint32_t  optimalLoadPercentage = 70;
    uint32_t  adjustmentSpeed = 1000;

public:
    DynamicPricingStrategy( const u256& minPrice, const u256& maxPrice,
        uint32_t optimalLoadPercentage, uint32_t adjustmentSpeed );

    u256 calculatePrice(u256 previousPrice, const ConsensusExtFace::transactions_vector &_approvedTransactions,
                        uint64_t _timeStamp, uint32_t  _timeStampMs, block_id _blockID) override;

};


#endif //SKALED_DYNAMICPRICINGSTRATEGY_H
