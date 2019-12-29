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

    @file NetworkMessage.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"

#include "Log.h"
#include "exceptions/FatalError.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/InvalidArgumentException.h"
#include "exceptions/InvalidSchainException.h"
#include "thirdparty/json.hpp"
#include "crypto/bls_include.h"
#include "protocols/ProtocolKey.h"
#include "protocols/binconsensus/BinConsensusInstance.h"
#include "chains/Schain.h"
#include "crypto/ConsensusBLSSigShare.h"
#include "crypto/CryptoManager.h"
#include "node/NodeInfo.h"
#include "network/Buffer.h"
#include "Message.h"
#include "network/TransportNetwork.h"
#include "NetworkMessageEnvelope.h"
#include "protocols/binconsensus/BVBroadcastMessage.h"
#include "protocols/binconsensus/AUXBroadcastMessage.h"
#include "protocols/blockconsensus/BlockSignBroadcastMessage.h"
#include "NetworkMessage.h"


NetworkMessage::NetworkMessage(MsgType _messageType, node_id _destinationNodeID,
                               block_id _blockID, schain_index _blockProposerIndex,
                               bin_consensus_round _r,
                               bin_consensus_value _value,
                               ProtocolInstance &_srcProtocolInstance)
        : Message(_srcProtocolInstance.getSchain()->getSchainID(),
                  _messageType, _srcProtocolInstance.createNetworkMessageID(),
                  _srcProtocolInstance.getSchain()->getNode()->getNodeID(), _destinationNodeID, _blockID,
                  _blockProposerIndex), BasicHeader(getTypeString(_messageType)) {

    this->srcSchainIndex = _srcProtocolInstance.getSchain()->getSchainIndex();
    this->r = _r;
    this->value = _value;

    auto ipString = _srcProtocolInstance.getSchain()->getThisNodeInfo()->getBaseIP();

    this->ip = inet_addr(ipString->c_str());

    setComplete();

}


NetworkMessage::NetworkMessage(MsgType _messageType, node_id _srcNodeID, node_id _dstNodeID, block_id _blockID,
                               schain_index _blockProposerIndex, bin_consensus_round _r, bin_consensus_value _value,
                               schain_id _schainId, msg_id _msgID, uint32_t _ip, ptr<string> _sigShareStr,
                               schain_index _srcSchainIndex, ptr<CryptoManager> _cryptoManager,
                               uint64_t _totalSigners, uint64_t _requiredSigners)
        : Message(_schainId, _messageType, _msgID, _srcNodeID, _dstNodeID, _blockID, _blockProposerIndex),
          BasicHeader(getTypeString(_messageType)) {

    ASSERT(_srcSchainIndex > 0)

    this->srcSchainIndex = _srcSchainIndex;
    this->r = _r;
    this->value = _value;
    this->ip = _ip;
    this->sigShareString = _sigShareStr;

    if (_sigShareStr != nullptr) {
        sigShare = _cryptoManager->createSigShare(_sigShareStr, _schainId, _blockID, _srcSchainIndex,
                                                  _totalSigners, _requiredSigners);
    }

    setComplete();

}

ptr<ThresholdSigShare> NetworkMessage::getSigShare() const {
    return sigShare;
}


bin_consensus_round NetworkMessage::getRound() const {
    return r;
}

bin_consensus_value NetworkMessage::getValue() const {
    return value;
}

void NetworkMessage::printMessage() {

    string s;


    cerr << "|" << printPrefix << ":" << r << ":v:" << to_string(uint8_t(value)) << "|";


}

void NetworkMessage::setIp(int32_t _ip) {
    ip = _ip;
}

void NetworkMessage::addFields(nlohmann::basic_json<> &j) {

    j["si"] = (uint64_t ) schainID;
    j["bi"] = (uint64_t )blockID;
    j["bpi"] = (uint64_t )getBlockProposerIndex();
    j["mt"] = (uint64_t )msgType;
    j["mi"] = (uint64_t )msgID;
    j["sni"] = (uint64_t )srcNodeID;
    j["dni"] = (uint64_t )dstNodeID;
    j["ssi"] = (uint64_t) srcSchainIndex;
    j["r"] = (uint64_t )r;
    j["v"] = (uint8_t )value;

    if (sigShareString != nullptr) {
        j["sss"] = *sigShareString;
    }
}


ptr<Buffer> NetworkMessage::toBuffer1() {


    static vector<uint8_t> ZERO_BUFFER(BLS_MAX_SIG_LEN, 0);

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

ptr<NetworkMessage> NetworkMessage::parseMessage(ptr<string> _header, Schain *_sChain) {

    uint64_t sChainID;
    uint64_t blockID;
    uint64_t blockProposerIndex;
    MsgType msgType;
    uint64_t msgID;
    uint64_t srcNodeID;
    uint64_t dstNodeID;
    uint64_t srcSchainIndex;
    uint64_t round;
    uint8_t value;
    uint32_t ip;
    ptr<string> sigShare;

    CHECK_ARGUMENT(_header);
    CHECK_ARGUMENT(_sChain);

    try {

        auto js = nlohmann::json::parse(*_header);







        sChainID = getUint64(js, "si");
        blockID = getUint64(js, "bi");
        blockProposerIndex = getUint64(js, "bpi");
        msgType = (MsgType) getUint64(js, "mt");
        msgID = getUint64(js, "mi");
        srcNodeID = getUint64(js, "sni");
        dstNodeID = getUint64(js, "dni");
        srcSchainIndex = getUint64(js, "ssi");
        round = getUint64(js, "r");
        value = getUint64(js, "v");

        if (js.find("sss") != js.end()) {
            sigShare = getString(js, "sss");
        }


    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(InvalidStateException("Could not parse message", __CLASS_NAME__));
    }

    try {

        if (_sChain->getSchainID() != sChainID) {
            BOOST_THROW_EXCEPTION(
                    InvalidSchainException("unknown Schain id" + to_string(sChainID), __CLASS_NAME__));
        }

        ptr<NetworkMessage> mptr;

        if (msgType == MsgType::MSG_BVB_BROADCAST) {
            mptr = make_shared<BVBroadcastMessage>(node_id(srcNodeID), node_id(dstNodeID),
                                                   block_id(blockID), schain_index(blockProposerIndex),
                                                   bin_consensus_round(round),
                                                   bin_consensus_value(value), schain_id(sChainID), msg_id(msgID),
                                                   ip,
                                                   srcSchainIndex,
                                                   _sChain);
        } else if (msgType == MsgType::MSG_AUX_BROADCAST) {
            mptr = make_shared<AUXBroadcastMessage>(node_id(srcNodeID), node_id(dstNodeID),
                                                    block_id(blockID), schain_index(blockProposerIndex),
                                                    bin_consensus_round(round),
                                                    bin_consensus_value(value), schain_id(sChainID), msg_id(msgID),
                                                    ip,
                                                    sigShare,
                                                    srcSchainIndex,
                                                    _sChain);
        } else if (msgType == MsgType::MSG_BLOCK_SIGN_BROADCAST) {
            mptr = make_shared<BlockSignBroadcastMessage>(node_id(srcNodeID), node_id(dstNodeID),
                                                          block_id(blockID), schain_index(blockProposerIndex),
                                                          schain_id(sChainID), msg_id(msgID), ip,
                                                          sigShare,
                                                          srcSchainIndex,
                                                          _sChain);
        } else {
            ASSERT(false);
        }

        return nullptr;

    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(InvalidStateException("Could not create message", __CLASS_NAME__));
    }
}


const char *NetworkMessage::getTypeString(MsgType _type) {
    switch (_type) {
        case MSG_BVB_BROADCAST : {
            return BV_BROADCAST;
        };
        case MSG_AUX_BROADCAST : {
            return AUX_BROADCAST;
        };
        case MSG_BLOCK_SIGN_BROADCAST : {
            return BLOCK_SIG_BROADCAST;
        }
        default: {
            return "history";
        };
    }

}




