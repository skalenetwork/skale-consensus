#pragma once



enum MsgType {CHILD_COMPLETED, PARENT_COMPLETED,

    BVB_BROADCAST, AUX_BROADCAST, BIN_CONSENSUS_COMMIT, BIN_CONSENSUS_HISTORY_DECIDE,
    BIN_CONSENSUS_HISTORY_CC, BIN_CONSENSUS_HISTORY_BVSELF, BIN_CONSENSUS_HISTORY_AUXSELF, BIN_CONSENSUS_HISTORY_NEW_ROUND,
    MSG_BLOCK_CONSENSUS_INIT, MSG_CONSENSUS_PROPOSAL };


class ProtocolInstance;
class ProtocolKey;

class Message {


protected:

    schain_id schainID;
    block_id  blockID;
    schain_index blockProposerIndex;
    MsgType msgType;
    msg_id msgID;
    node_id srcNodeID;
    node_id dstNodeID;

    ptr<ProtocolKey> protocolKey;

public:
    Message(const schain_id &schainID, MsgType msgType, const msg_id &msgID, const node_id &srcNodeID,
            const node_id &dstNodeID, const block_id &blockID = block_id(0),
            const schain_index &blockProposerIndex = schain_index(0));

    node_id getSrcNodeID() const;

    node_id getDstNodeID() const;

    msg_id getMessageID() const;

    MsgType getMessageType() const;

    const block_id getBlockId() const;

    const schain_index &getBlockProposerIndex() const ;

    schain_id getSchainID() const;


    ptr<ProtocolKey> createDestinationProtocolKey();

    void setSrcNodeID(const node_id &srcNodeID);

    void setDstNodeID(const node_id &dstNodeID);

    const block_id &getBlockID() const;

    MsgType getMsgType() const;

    const msg_id &getMsgID() const;

    virtual ~Message();


    static uint64_t getTotalObjects() {
        return totalObjects;
    }

private:


    static atomic<uint64_t>  totalObjects;

};
