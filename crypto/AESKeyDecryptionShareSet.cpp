#include "SkaleCommon.h"
#include "Log.h"
#include "AESKeyDecryptionShareSet.h"

AESKeyDecryptionShareSet::AESKeyDecryptionShareSet( const block_id _blockId, transaction_index _transactionIndex )
    : blockId( _blockId ), transactionIndex( _transactionIndex ) {
    totalObjects++;
};

AESKeyDecryptionShareSet::~AESKeyDecryptionShareSet() {
    totalObjects--;
}

atomic< int64_t > AESKeyDecryptionShareSet::totalObjects = 0;
