#pragma  once




#include "../../messages/NetworkMessage.h"


class BinConsensusInstance;


class AUXBroadcastMessage : public NetworkMessage{
public:



    AUXBroadcastMessage(bin_consensus_round round, bin_consensus_value value, node_id destinationNodeID,
                        block_id _blockID, schain_index _proposerIndex,
                        BinConsensusInstance &sourceProtocolInstance);

    AUXBroadcastMessage(node_id _srcNodeID, node_id _dstNodeID,
            block_id _blockID, schain_index _blockProposerIndex,
            bin_consensus_round _r, bin_consensus_value _value,
            schain_id _schainId, msg_id _msgID, uint32_t _ip);

};
