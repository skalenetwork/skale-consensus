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

    @file PricingAgent.cpp
    @author Stan Kladko
    @date 2019
*/

#include "../SkaleCommon.h"
#include "../thirdparty/json.hpp"
#include <boost/multiprecision/cpp_int.hpp>
#include "../Log.h"
#include "../node/Node.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/ParsingException.h"
#include "../chains/Schain.h"
#include "../node/ConsensusEngine.h"
#include "PricingStrategy.h"
#include "DynamicPricingStrategy.h"
#include "ZeroPricingStrategy.h"
#include "../db/LevelDB.h"

#include "PricingAgent.h"

PricingAgent::PricingAgent(Schain &_sChain) : Agent(_sChain, false) {

    string def("DYNAMIC");

    auto strategy = _sChain.getNode()->getParamString("pricingStrategy", def);

   if (*strategy == "DYNAMIC") {
       pricingStrategy = make_shared<DynamicPricingStrategy>();
   } else if (*strategy == "ZERO") {
       pricingStrategy = make_shared<ZeroPricingStrategy>();
   } else {
       BOOST_THROW_EXCEPTION(ParsingException("Unknown pricing strategy: " + *strategy , __CLASS_NAME__ ));
   }
}

u256
PricingAgent::calculatePrice(const ConsensusExtFace::transactions_vector &_approvedTransactions, uint64_t _timeStamp,
                             uint32_t _timeStampMs,
                             block_id _blockID) {
    u256  price;

    ASSERT(pricingStrategy != nullptr);

    if (_blockID <= 1) {
        price = 1000;
    } else {
        auto oldPrice = readPrice(_blockID - 1);
        price = pricingStrategy->calculatePrice(oldPrice, _approvedTransactions, _timeStamp,
            _timeStampMs,
            _blockID);
    }


    savePrice(price, _blockID);
    return price;

}


void PricingAgent::savePrice(u256 _price, block_id _blockID) {


    auto db = sChain->getNode()->getPricesDB();

    auto  key = to_string(_blockID);

    auto value = _price.str();

    db->writeString(key, value);


}


u256 PricingAgent::readPrice(block_id _blockID) {

    auto db = sChain->getNode()->getPricesDB();

    auto  key = to_string(_blockID);

    auto price = db->readString(key);

    ASSERT(price != nullptr);

    return u256(price->c_str());
}

