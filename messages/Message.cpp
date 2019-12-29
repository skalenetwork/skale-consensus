/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file Message.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"
#include "protocols/ProtocolInstance.h"
#include "protocols/ProtocolKey.h"

#include "Message.h"


ptr<ProtocolKey> Message::createDestinationProtocolKey()  {
    ASSERT(msgType == PARENT_COMPLETED || msgType == MSG_BVB_BROADCAST ||
           msgType == MSG_AUX_BROADCAST || msgType == BIN_CONSENSUS_COMMIT || msgType == MSG_BLOCK_SIGN_BROADCAST);
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




const block_id Message::getBlockId() const {

    return blockID;
}

schain_index Message::getBlockProposerIndex() const {
    return blockProposerIndex;
}





Message::Message(const schain_id &schainID, MsgType msgType, const msg_id &msgID, const node_id &srcNodeID,
                 const block_id &blockID, const schain_index &blockProposerIndex) : schainID(schainID),
                                                                                    blockID(blockID),
                                                                                    blockProposerIndex(blockProposerIndex),
                                                                                    msgType(msgType), msgID(msgID),

                                                                                    srcNodeID(srcNodeID) {
    if ((uint64_t)blockID == 0) {
        ASSERT(false);
    }
    totalObjects++;
}

void Message::setSrcNodeID(const node_id &_srcNodeID) {
    Message::srcNodeID = _srcNodeID;
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
