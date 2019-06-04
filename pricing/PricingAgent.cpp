//
// Created by skale on 6/2/19.
//

#include "../SkaleCommon.h"
#include <boost/multiprecision/cpp_int.hpp>
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../chains/Schain.h"
#include "../node/ConsensusEngine.h"
#include "PricingStrategy.h"
#include "DynamicPricingStrategy.h"
#include "ZeroPricingStrategy.h"
#include "../db/LevelDB.h"

#include "PricingAgent.h"

PricingAgent::PricingAgent(Schain &_sChain) : Agent(_sChain, false) {
    pricingStrategy = make_shared<DynamicPricingStrategy>();
}

u256
PricingAgent::calculatePrice(const ConsensusExtFace::transactions_vector &_approvedTransactions, uint64_t _timeStamp,
                             block_id _blockID) {
    u256  price;

    ASSERT(pricingStrategy != nullptr);

    if (_blockID <= 1) {
        price = 1000;
    } else {
        auto oldPrice = readPrice(_blockID - 1);
        price = pricingStrategy->calculatePrice(oldPrice, _approvedTransactions, _timeStamp, _blockID);
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

