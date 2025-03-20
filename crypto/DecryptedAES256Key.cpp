
#include "SkaleCommon.h"

#include "network/Utils.h"

#include "DecryptedAES256Key.h"

block_id DecryptedAES256Key::getBlockId() const {
    return blockId;
}

DecryptedAES256Key::~DecryptedAES256Key() {}

DecryptedAES256Key::DecryptedAES256Key(
    const block_id& blockId, uint64_t _totalDecryptors, uint64_t _requiredDecryptors )
    : blockId( blockId ), totalNodes( _totalDecryptors ), requiredNodes( _requiredDecryptors ) {}
