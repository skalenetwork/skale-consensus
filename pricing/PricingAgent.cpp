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

    @file PricingAgent.cpp
    @author Stan Kladko
    @date 2019
*/

#include "SkaleCommon.h"
#include "Log.h"

#include "DynamicPricingStrategy.h"


#include "ZeroPricingStrategy.h"
#include "chains/Schain.h"
#include "db/PriceDB.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/FatalError.h"
#include "exceptions/ParsingException.h"
#include "node/ConsensusEngine.h"
#include "thirdparty/json.hpp"
#include <boost/multiprecision/cpp_int.hpp>


#include "PricingAgent.h"

PricingAgent::PricingAgent(Schain &_sChain) : Agent(_sChain, false) {

    string def("DYNAMIC");

    auto strategy = _sChain.getNode()->getParamString("pricingStrategy", def);
    CHECK_STATE(!strategy.empty())

   if (strategy == "DYNAMIC") {

       u256 minPrice = sChain->getNode()->getParamUint64(string("DYNAMIC_PRICING_MIN_PRICE"),
           DEFAULT_MIN_PRICE);
       u256  maxPrice =- sChain->getNode()->getParamUint64("DYNAMIC_PRICING_MAX_PRICE", 1000000000);
       uint64_t  optimalLoadPercentage = sChain->getNode()->getParamUint64("DYNAMIC_PRICING_OPTIMAL_LOAD_PERCENTAGE", 70);
       uint64_t adjustmentSpeed = sChain->getNode()->getParamUint64("DYNAMIC_PRICING_ADJUSTMENT_SPEED", 1000);
       pricingStrategy = make_shared<DynamicPricingStrategy>(minPrice, maxPrice, optimalLoadPercentage, adjustmentSpeed);

   } else if (strategy == "ZERO") {
       pricingStrategy = make_shared<ZeroPricingStrategy>();
   } else {
       BOOST_THROW_EXCEPTION(ParsingException("Unknown pricing strategy: " + strategy , __CLASS_NAME__ ));
   }
}

u256
PricingAgent::calculatePrice(const ConsensusExtFace::transactions_vector &_approvedTransactions, uint64_t _timeStamp,
                             uint32_t _timeStampMs,
                             block_id _blockID) {


    u256  price;
    CHECK_STATE(pricingStrategy);
    try {
        if (_blockID <= 1) {
            price = sChain->getNode()->getParamUint64(string("DYNAMIC_PRICING_START_PRICE"),
                DEFAULT_MIN_PRICE);
        } else {
            auto oldPrice = readPrice(_blockID - 1);
            price = pricingStrategy->calculatePrice(oldPrice, _approvedTransactions, _timeStamp,
                                                    _timeStampMs, _blockID);
        }

        savePrice(price, _blockID);

    } catch (ExitRequestedException&) {throw;} catch(...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

    return price;

}


void PricingAgent::savePrice(u256 _price, block_id _blockID) {
    auto db = sChain->getNode()->getPriceDB();
    CHECK_STATE(db);
    db->savePrice(_price, _blockID);
}


u256 PricingAgent::readPrice(block_id _blockID) {
    auto db = sChain->getNode()->getPriceDB();
    CHECK_STATE(db);
    return db->readPrice(_blockID);
}

