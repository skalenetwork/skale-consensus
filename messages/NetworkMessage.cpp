//
// Created by stan on 19.02.18.
//

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../thirdparty/json.hpp"
#include "../protocols/ProtocolKey.h"
#include "../protocols/binconsensus/BinConsensusInstance.h"
#include "../chains/Schain.h"
#include "../node/Node.h"
#include "../node/NodeInfo.h"
#include "../network/Buffer.h"
#include "Message.h"
#include "NetworkMessageEnvelope.h"
#include "NetworkMessage.h"



NetworkMessage::NetworkMessage(MsgType _messageType, node_id _destinationNodeID,
                               block_id _blockID, schain_index _blockProposerIndex,
                               bin_consensus_round _r,
                               bin_consensus_value _value,
                               BinConsensusInstance &_srcProtocolInstance)
        : Message(_srcProtocolInstance.getSchain()->getSchainID(),
                _messageType, _srcProtocolInstance.createNetworkMessageID(),
                  _srcProtocolInstance.getSchain()->getNode()->getNodeID(), _destinationNodeID, _blockID, _blockProposerIndex) {

     this->r = _r;
     this->value = _value;

     auto ipString = _srcProtocolInstance.getSchain()->getThisNodeInfo()->getBaseIP();


     this->ip = inet_addr(ipString->c_str());

     ASSERT(_messageType > 0);


}



NetworkMessage::NetworkMessage(MsgType messageType, node_id _srcNodeID, node_id _dstNodeID,
                               block_id _blockID, schain_index _blockProposerIndex,
                               bin_consensus_round _r,
                               bin_consensus_value _value,
                               schain_id _schainId, msg_id _msgID, uint32_t _ip)
        : Message(_schainId, messageType, _msgID, _srcNodeID,_dstNodeID, _blockID, _blockProposerIndex) {

    this->r = _r;
    this->value = _value;
    this->ip = _ip;
    ASSERT(messageType > 0);
}




bin_consensus_round NetworkMessage::getRound() const {
    return r;
}

bin_consensus_value NetworkMessage::getValue() const {
    return value;
}

int32_t NetworkMessage::getIp() const {
    return ip;
}

msg_len NetworkMessage::getLen() {
    return msg_len(sizeof(NetworkMessage));
}

void NetworkMessage::printMessage() {

    string s;


    cerr << "|" << printPrefix << ":"  << r << ":v:" << to_string(uint8_t(value)) << "|";


}

void NetworkMessage::setIp(int32_t _ip) {
    ip = _ip;
}

ptr<Buffer> NetworkMessage::toBuffer() {

    ASSERT(getSrcNodeID() != getDstNodeID());



    auto buf = make_shared<Buffer>(sizeof(NetworkMessage));

    static uint64_t magic = MAGIC_NUMBER;

    WRITE(buf, magic);
    WRITE(buf, schainID);
    WRITE(buf, blockID);
    WRITE(buf, blockProposerIndex);
    WRITE(buf, msgType);
    WRITE(buf, msgID);
    WRITE(buf, srcNodeID);
    WRITE(buf, dstNodeID);
    WRITE(buf, r);
    WRITE(buf, value);
    WRITE(buf, ip);

    return buf;
}








