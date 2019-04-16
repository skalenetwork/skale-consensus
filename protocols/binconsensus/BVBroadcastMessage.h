#pragma  once





class BinConsensusInstance;


class BVBroadcastMessage : public NetworkMessage{
public:




    BVBroadcastMessage(node_id destinationNodeID, block_id _blockID,
                           schain_index _blockProposerIndex, bin_consensus_round r,
                           bin_consensus_value value, BinConsensusInstance &sourceProtocolInstance);

    BVBroadcastMessage(node_id _srcNodeID, node_id _dstNodeID,
                                           block_id _blockID, schain_index _blockProposerIndex,
                                           bin_consensus_round _r,
                                           bin_consensus_value _value,
                                           schain_id _schainId, msg_id _msgID, uint32_t _ip);


};

