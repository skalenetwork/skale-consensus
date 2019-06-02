//
// Created by skale on 6/2/19.
//

#include "SkaleConfig.h"
#include "ZeroPricingStrategy.h"

u256 ZeroPricingStrategy::calculatePrice(u256,
                                         const ConsensusExtFace::transactions_vector &,
                                         uint64_t, uint64_t) {
    return 0;
}
