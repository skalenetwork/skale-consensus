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

    @file DynamicPricingStrategy.cpp
    @author Stan Kladko
    @date 2019
*/

#include "SkaleCommon.h"
#include "node/ConsensusEngine.h"
#include "DynamicPricingStrategy.h"


u256 DynamicPricingStrategy::calculatePrice( u256 _previousPrice,
    const ConsensusExtFace::transactions_vector& _block, uint64_t, uint32_t, block_id ) {
    auto loadPercentage = ( _block.size() * 100 ) / MAX_TRANSACTIONS_PER_BLOCK;

    u256 price;

    if ( loadPercentage < optimalLoadPercentage ) {
        price = _previousPrice - ( adjustmentSpeed * _previousPrice ) *
                                     ( optimalLoadPercentage - loadPercentage ) / 1000000;

    } else {
        price = _previousPrice + ( adjustmentSpeed * _previousPrice ) *
                                     ( loadPercentage - optimalLoadPercentage ) / 1000000;
    }

    if ( price < minPrice ) {
        price = minPrice;
    }

    if ( price > maxPrice ) {
        price = maxPrice;
    }

    return price;
}
DynamicPricingStrategy::DynamicPricingStrategy( const u256& minPrice, const u256& maxPrice,
    uint32_t optimalLoadPercentage, uint32_t adjustmentSpeed )
    : minPrice( minPrice ),
      maxPrice( maxPrice ),
      optimalLoadPercentage( optimalLoadPercentage ),
      adjustmentSpeed( adjustmentSpeed ){};
