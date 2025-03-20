#pragma once

class ThresholdSignature;
class ThresholdSigShare;

class ThresholdSigShareSet {
public:
    ThresholdSigShareSet( block_id _blockId, uint64_t _totalSigners, uint64_t _requiredSigners );

protected:
    block_id blockId = 0;
    uint64_t totalSigners = 0;
    uint64_t requiredSigners = 0;

    static atomic< int64_t > totalObjects;

public:
    virtual ~ThresholdSigShareSet();

    static int64_t getTotalObjects();

    virtual ptr< ThresholdSignature > mergeSignature() = 0;

    virtual bool isEnough() = 0;

    virtual bool addSigShare( const ptr< ThresholdSigShare >& _sigShare ) = 0;
};


#endif  // SKALED_THRESHOLDSIGSHARESET_H
