//
// Created by skale on 6/2/19.
//

#ifndef SKALED_ZEROPRICESTRATEGY_H
#define SKALED_ZEROPRICINGSTRATEGY_H

#include "PricingStrategy.h"
#include "../SkaleCommon.h"
#include "ConsensusEngine.h"

class ZeroPricingStrategy : PricingStrategy{

    u256 calculatePrice(u256 previousPrice, const ConsensusExtFace::transactions_vector &_approvedTransactions,
                        uint64_t _timeStamp, uint64_t _blockID) override;

};


#endif //SKALED_ZEROPRICESTRATEGY_H
