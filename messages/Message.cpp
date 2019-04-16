
#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../protocols/ProtocolInstance.h"
#include "../protocols/ProtocolKey.h"

#include "Message.h"


ptr<ProtocolKey> Message::createDestinationProtocolKey()  {
    ASSERT(msgType == PARENT_COMPLETED ||  msgType ==  BVB_BROADCAST ||
           msgType == AUX_BROADCAST || msgType == BIN_CONSENSUS_COMMIT);


    ASSERT(blockID > 0);
    if (protocolKey == nullptr) {
        protocolKey = make_shared<ProtocolKey>(blockID, blockProposerIndex);
    }
    return protocolKey;

}


msg_id Message::getMessageID() const {
    return msgID;
}

MsgType Message::getMessageType() const {
    return msgType;
}


schain_id Message::getSchainID() const {
    return schainID;
}






node_id Message::getSrcNodeID() const {
    return srcNodeID;
}


node_id Message::getDstNodeID() const {
    return dstNodeID;
}

const block_id Message::getBlockId() const {

    return blockID;
}

const schain_index &Message::getBlockProposerIndex() const {
    return blockProposerIndex;
}





Message::Message(const schain_id &_schainID, MsgType _msgType, const msg_id &_msgID,
                 const node_id &_srcNodeID, const node_id &_dstNodeID, const block_id &_blockID,
                 const schain_index &_blockProposerIndex) : schainID(_schainID),
                                                                       blockID(_blockID),
                                                                       blockProposerIndex(_blockProposerIndex),
                                                                       msgType(_msgType), msgID(_msgID),

                                                                       srcNodeID(_srcNodeID), dstNodeID(_dstNodeID) {
    if ((uint64_t)blockID == 0) {
        ASSERT(false);
    }
    totalObjects++;
}

void Message::setSrcNodeID(const node_id &srcNodeID) {
    Message::srcNodeID = srcNodeID;
}

void Message::setDstNodeID(const node_id &dstNodeID) {
    Message::dstNodeID = dstNodeID;
}

const block_id &Message::getBlockID() const {
    return blockID;
}

MsgType Message::getMsgType() const {
    return msgType;
}

const msg_id &Message::getMsgID() const {
    return msgID;
}


Message::~Message() {
    totalObjects--;
}


atomic<uint64_t>  Message::totalObjects(0);
