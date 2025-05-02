#include "SkaleCommon.h"
#include "Log.h"
#include "AESKeyDecryptionShareSet.h"

AESKeyDecryptionShareSet::AESKeyDecryptionShareSet( const block_id _blockId,
    transaction_index _transactionIndex, uint64_t _totalDecryptors, uint64_t _requiredDecryptors )
    : blockId( _blockId ),
      transactionIndex( _transactionIndex ),
      totalDecryptors( _totalDecryptors ),
      requiredDecryptors( _requiredDecryptors ) {
    totalObjects++;
};

AESKeyDecryptionShareSet::~AESKeyDecryptionShareSet() {
    totalObjects--;
}

atomic< int64_t > AESKeyDecryptionShareSet::totalObjects = 0;
