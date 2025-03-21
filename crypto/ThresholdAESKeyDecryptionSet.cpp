#include "SkaleCommon.h"

#include "ThresholdAESKeyDecryptionShareSet.h"

ThresholdAESKeyDecryptionShareSet::ThresholdAESKeyDecryptionShareSet( const block_id _blockId,
    transaction_index _transactionIndex, uint64_t _totalDecryptors, uint64_t _requiredDecryptors )
    : blockId( _blockId ),
      transactionIndex( _transactionIndex ),
      totalDecryptors( _totalDecryptors ),
      requiredDecryptors( _requiredDecryptors ) {};

ThresholdAESKeyDecryptionShareSet::~ThresholdAESKeyDecryptionShareSet() = default;
