#include "SkaleCommon.h"

#include "ThresholdAESKeyDecryptionShareSet.h"

atomic< int64_t > ThresholdAESKeyDecryptionShareSet::totalObjects( 0 );

int64_t ThresholdAESKeyDecryptionShareSet::getTotalObjects() {
    return totalObjects;
}

ThresholdAESKeyDecryptionShareSet::ThresholdAESKeyDecryptionShareSet(
    const block_id _blockId, uint64_t _totalDecryptors, uint64_t _requiredDecryptors )
    : blockId( _blockId ), totalDecryptors( _totalDecryptors ), requiredDecryptors( _requiredDecryptors ) {}

ThresholdAESKeyDecryptionShareSet::~ThresholdAESKeyDecryptionShareSet() {}
