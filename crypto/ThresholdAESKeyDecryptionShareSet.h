#pragma once

class AESKey;
class ThresholdAESKeyDecryptionShare;

class ThresholdAESKeyDecryptionShareSet {
public:
    ThresholdAESKeyDecryptionShareSet( block_id _blockId, uint64_t _totalDecryptors, uint64_t _requiredDecryptors );

protected:
    block_id blockId = 0;
    uint64_t totalDecryptors = 0;
    uint64_t requiredDecryptors = 0;

    static atomic< int64_t > totalObjects;

public:
    virtual ~ThresholdAESKeyDecryptionShareSet();

    static int64_t getTotalObjects();

    virtual ptr< AESKey > mergeAESKey() = 0;

    virtual bool isEnough() = 0;

    virtual bool addDecryptionhare( const ptr< ThresholdAESKeyDecryptionShare >& _decryptionShare ) = 0;
};



