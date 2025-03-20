
#include "SkaleCommon.h"

#include "network/Utils.h"

#include "DecryptedAESKey.h"

block_id DecryptedAESKey::getBlockId() const {
    return blockId;
}

DecryptedAESKey::~DecryptedAESKey() {}

DecryptedAESKey::DecryptedAESKey(
    const block_id& blockId, uint64_t _totalDecryptors, uint64_t _requiredDecryptors )
    : blockId( blockId ),
      totalDecryptors( _totalDecryptors ),
      requiredDecryptors( _requiredDecryptors ) {}
