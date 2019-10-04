//
// Created by kladko on 04.10.19.
//

#ifndef SKALED_THRESHOLDSIGSHARESET_H
#define SKALED_THRESHOLDSIGSHARESET_H


class ThresholdSigShare;
class ThresholdSignature;

class ThresholdSigShareSet {

protected:
    block_id blockId;
    uint64_t requiredSigners;
    static atomic<uint64_t>  totalObjects;
    uint64_t totalSigners;
public:
    ThresholdSigShareSet(const block_id &blockId, uint64_t requiredSigners, uint64_t totalSigners);

    virtual bool isEnough() = 0;

    virtual bool isEnoughMinusOne() = 0;

    static uint64_t getTotalObjects();

    virtual bool addSigShare(std::shared_ptr<ThresholdSigShare> _sigShare) = 0;


    virtual ptr<ThresholdSignature> mergeSignature() = 0;



};


#endif //SKALED_THRESHOLDSIGSHARESET_H
