#include "SkaleCommon.h"

#include "ThresholdAESKeyDecryptionShareSet.h"

ThresholdAESKeyDecryptionShareSet::ThresholdAESKeyDecryptionShareSet(
    const block_id _blockId, uint64_t _totalDecryptors, uint64_t _requiredDecryptors )
    : blockId( _blockId ), totalDecryptors( _totalDecryptors ), requiredDecryptors( _requiredDecryptors ){};

ThresholdAESKeyDecryptionShareSet::~ThresholdAESKeyDecryptionShareSet()  = default;

