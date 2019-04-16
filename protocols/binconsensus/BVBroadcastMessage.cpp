#include "../../SkaleConfig.h"
#include "../../Log.h"
#include "../../exceptions/FatalError.h"

#include "../../messages/NetworkMessage.h"

#include "../ProtocolKey.h"
#include "../ProtocolInstance.h"
#include "BinConsensusInstance.h"

#include "BVBroadcastMessage.h"


BVBroadcastMessage::BVBroadcastMessage(node_id destinationNodeID, block_id _blockID,
                                       schain_index _blockProposerIndex, bin_consensus_round r,
                                       bin_consensus_value value, BinConsensusInstance &sourceProtocolInstance)
        : NetworkMessage(BVB_BROADCAST, destinationNodeID, _blockID, _blockProposerIndex, r, value,
                         sourceProtocolInstance) {
    printPrefix = "b";
}


BVBroadcastMessage::BVBroadcastMessage(node_id _srcNodeID, node_id _dstNodeID,
                                       block_id _blockID, schain_index _blockProposerIndex,
                                       bin_consensus_round _r,
                                       bin_consensus_value _value,
                                       schain_id _schainId, msg_id _msgID, uint32_t _ip) : NetworkMessage(
        BVB_BROADCAST, _srcNodeID, _dstNodeID, _blockID, _blockProposerIndex, _r, _value, _schainId, _msgID, _ip) {
    printPrefix = "b";
};
