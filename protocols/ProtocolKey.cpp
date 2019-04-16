#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../messages/NetworkMessage.h"
#include "ProtocolInstance.h"
#include "ProtocolKey.h"






ProtocolKey::ProtocolKey(const ProtocolKey &key) :
        blockID(key.blockID),
        blockProposerIndex(key.blockProposerIndex) {
    ASSERT((uint64_t) blockID > 0);
}

block_id ProtocolKey::getBlockID() const {
    ASSERT((uint64_t) blockID > 0);
    return blockID;
}

schain_index ProtocolKey::getBlockProposerIndex() const {

    return blockProposerIndex;
}

ProtocolKey::ProtocolKey(block_id _blockId, schain_index _blockProposerIndex) :
        blockID(_blockId),  blockProposerIndex(_blockProposerIndex){
    ASSERT((uint64_t) blockID > 0);
}

bool operator<(const ProtocolKey &l, const ProtocolKey &r) {

    if ((uint64_t )l.getBlockID() != (uint64_t ) r.getBlockID()) {
        return (uint64_t )l.getBlockID() < (uint64_t ) r.getBlockID();
    }

    return (uint64_t )l.getBlockProposerIndex() < (uint64_t )r.getBlockProposerIndex();

}
