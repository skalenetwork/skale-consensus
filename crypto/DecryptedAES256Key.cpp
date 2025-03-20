
#include "SkaleCommon.h"

#include "network/Utils.h"

#include "DecryptedAES256Key.h"

block_id DecryptedAES256Key::getBlockId() const {
    return blockId;
}

DecryptedAES256Key::~DecryptedAES256Key() {}

DecryptedAES256Key::DecryptedAES256Key(
    const block_id& blockId, uint64_t _totalNodes, uint64_t _requiredNodes )
    : blockId( blockId ), totalNodes( _totalNodes ), requiredNodes( _requiredNodes ) {}
