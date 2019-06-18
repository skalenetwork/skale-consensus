/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file NetworkMessage.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/InvalidArgumentException.h"
#include "../thirdparty/json.hpp"
#include "../protocols/ProtocolKey.h"
#include "../protocols/binconsensus/BinConsensusInstance.h"
#include "../chains/Schain.h"
#include "../crypto/BLSSigShare.h"
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



NetworkMessage::NetworkMessage(MsgType messageType, node_id _srcNodeID, node_id _dstNodeID, block_id _blockID,
                               schain_index _blockProposerIndex, bin_consensus_round _r, bin_consensus_value _value,
                               schain_id _schainId, msg_id _msgID, uint32_t _ip, ptr<string> _signature,
                               schain_index _srcSchainIndex)
        : Message(_schainId, messageType, _msgID, _srcNodeID,_dstNodeID, _blockID, _blockProposerIndex) {

    ASSERT(_srcSchainIndex > 0)

    this->r = _r;
    this->value = _value;
    this->ip = _ip;
    this->sigShareString = _signature;


    if (_signature->size() > BLS_MAX_SIG_LEN) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Signature size too large:" + *_signature, __CLASS_NAME__));
    }




    if (_signature->size() > 0 ) {
       sigShare = make_shared<BLSSigShare>(_signature, _schainId, _blockID, _srcSchainIndex, _srcNodeID);
    }



    ASSERT(messageType > 0);
}

const ptr<BLSSigShare> &NetworkMessage::getSigShare() const {
    return sigShare;
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
    return CONSENSUS_MESSAGE_LEN;
}

void NetworkMessage::printMessage() {

    string s;


    cerr << "|" << printPrefix << ":"  << r << ":v:" << to_string(uint8_t(value)) << "|";


}

void NetworkMessage::setIp(int32_t _ip) {
    ip = _ip;
}




ptr<Buffer> NetworkMessage::toBuffer() {

    static vector<uint8_t> ZERO_BUFFER(BLS_MAX_SIG_LEN, 0);

    ASSERT(getSrcNodeID() != getDstNodeID());


    auto buf = make_shared<Buffer>(CONSENSUS_MESSAGE_LEN);

    static uint64_t magic = MAGIC_NUMBER;

    WRITE(buf, magic);
    WRITE(buf, schainID);
    WRITE(buf, blockID);
    auto bpi = getBlockProposerIndex();
    WRITE(buf, bpi);
    WRITE(buf, msgType);
    WRITE(buf, msgID);
    WRITE(buf, srcNodeID);
    WRITE(buf, dstNodeID);
    WRITE(buf, r);
    WRITE(buf, value);
    WRITE(buf, ip);

    if (sigShareString != nullptr) {
        buf->write(sigShareString->data(), sigShareString->size());
    }

    if (buf->getCounter() < CONSENSUS_MESSAGE_LEN) {
        buf->write(ZERO_BUFFER.data(), CONSENSUS_MESSAGE_LEN - buf->getCounter());
    }

    return buf;
}








