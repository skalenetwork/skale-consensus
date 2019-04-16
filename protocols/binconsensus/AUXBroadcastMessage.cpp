//
// Created by stan on 19.02.18.
//


#include "../../SkaleConfig.h"
#include "../../Log.h"
#include "../../exceptions/FatalError.h"

#include "../../messages/NetworkMessage.h"
#include "../ProtocolKey.h"
#include "../ProtocolInstance.h"
#include "BinConsensusInstance.h"

#include "AUXBroadcastMessage.h"


AUXBroadcastMessage::AUXBroadcastMessage( bin_consensus_round round, bin_consensus_value value,
    node_id destinationNodeID, block_id _blockID, schain_index _blockProposer,
    BinConsensusInstance& sourceProtocolInstance )
    : NetworkMessage( AUX_BROADCAST, destinationNodeID, _blockID, _blockProposer, round, value,
          sourceProtocolInstance ) {
    printPrefix = "a";
}


AUXBroadcastMessage::AUXBroadcastMessage(node_id _srcNodeID, node_id _dstNodeID,
                                       block_id _blockID, schain_index _blockProposerIndex,
                                       bin_consensus_round _r,
                                       bin_consensus_value _value,
                                       schain_id _schainId, msg_id _msgID, uint32_t _ip) : NetworkMessage(
        AUX_BROADCAST, _srcNodeID, _dstNodeID, _blockID, _blockProposerIndex, _r, _value, _schainId, _msgID, _ip) {
    printPrefix = "a";

};
