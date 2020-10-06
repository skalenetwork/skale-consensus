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

#include "Message.h"
#include "NetworkMessage.h"
#include "NetworkMessageEnvelope.h"
#include "Log.h"
#include "chains/Schain.h"
#include "crypto/ConsensusBLSSigShare.h"
#include "crypto/CryptoManager.h"
#include "crypto/bls_include.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/FatalError.h"
#include "exceptions/InvalidArgumentException.h"
#include "exceptions/InvalidSchainException.h"
#include "network/Buffer.h"
#include "network/Network.h"
#include "node/NodeInfo.h"
#include "protocols/ProtocolKey.h"
#include "protocols/binconsensus/AUXBroadcastMessage.h"
#include "protocols/binconsensus/BVBroadcastMessage.h"
#include "protocols/binconsensus/BinConsensusInstance.h"
#include "protocols/blockconsensus/BlockSignBroadcastMessage.h"
#include "thirdparty/json.hpp"
#include "utils/Time.h"
#include <crypto/SHAHash.h>

#include "thirdparty/rapidjson/document.h"
#include "thirdparty/rapidjson/writer.h"
#include "thirdparty/rapidjson/stringbuffer.h"


NetworkMessage::NetworkMessage(MsgType _messageType, block_id _blockID, schain_index _blockProposerIndex,
                               bin_consensus_round _r,
                               bin_consensus_value _value, uint64_t _timeMs, ProtocolInstance &_srcProtocolInstance)
        : Message(_srcProtocolInstance.getSchain()->getSchainID(),
                  _messageType, _srcProtocolInstance.createNetworkMessageID(),
                  _srcProtocolInstance.getSchain()->getNode()->getNodeID(), _blockID,
                  _blockProposerIndex), BasicHeader(getTypeString(_messageType)) {


    this->srcSchainIndex = _srcProtocolInstance.getSchain()->getSchainIndex();
    this->r = _r;
    this->value = _value;
    this->timeMs = _timeMs;
    setComplete();
}


NetworkMessage::NetworkMessage(MsgType _messageType, node_id _srcNodeID, block_id _blockID,
                               schain_index _blockProposerIndex,
                               bin_consensus_round _r, bin_consensus_value _value, uint64_t _timeMs,
                               schain_id _schainId, msg_id _msgID, ptr<string> _sigShareStr, ptr<string> _ecdsaSig,
                               ptr<string> _publicKey, ptr<string> _pkSig,
                               schain_index _srcSchainIndex, ptr<CryptoManager> _cryptoManager)
        : Message(_schainId, _messageType, _msgID, _srcNodeID, _blockID, _blockProposerIndex),
          BasicHeader(getTypeString(_messageType)) {

    CHECK_ARGUMENT(_srcSchainIndex > 0)
    CHECK_ARGUMENT(_ecdsaSig)
    CHECK_ARGUMENT(_publicKey)
    CHECK_ARGUMENT(_pkSig)
    // STRANGE
    //CHECK_ARGUMENT(_sigShareStr);
    CHECK_ARGUMENT(_cryptoManager)
    CHECK_ARGUMENT(_timeMs > 0);

    this->srcSchainIndex = _srcSchainIndex;
    this->r = _r;
    this->value = _value;
    this->timeMs = _timeMs;
    this->sigShareString = _sigShareStr;
    this->ecdsaSig = _ecdsaSig;
    this->publicKey = _publicKey;
    this->pkSig = _pkSig;



    if (_sigShareStr != nullptr) {
        sigShare = _cryptoManager->createSigShare(_sigShareStr, _schainId, _blockID, _srcSchainIndex);
        CHECK_STATE(sigShare);
    }

    setComplete();

}
const ptr< string >& NetworkMessage::getPublicKey() const {
    return publicKey;
}
const ptr< string >& NetworkMessage::getPkSig() const {
    return pkSig;
}

ptr<ThresholdSigShare> NetworkMessage::getSigShare() const {
    CHECK_STATE(sigShare);
    return sigShare;
}

const ptr<string> &NetworkMessage::getECDSASig() const {
    CHECK_STATE(ecdsaSig);
    return ecdsaSig;
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


void NetworkMessage::addFields(nlohmann::basic_json<> &j) {

    j["si"] = (uint64_t ) schainID;
    j["bi"] = (uint64_t )blockID;
    j["bpi"] = (uint64_t )getBlockProposerIndex();
    j["mt"] = (uint64_t )msgType;
    j["mi"] = (uint64_t )msgID;
    j["sni"] = (uint64_t )srcNodeID;
    j["ssi"] = (uint64_t) srcSchainIndex;
    j["r"] = (uint64_t )r;
    j["t"] = (uint64_t) timeMs;
    j["v"] = (uint8_t )value;

    if (sigShareString) {
        j["sss"] = *sigShareString;
    }

    CHECK_STATE(ecdsaSig);
    j["sig"] = *ecdsaSig;
    j["pk"] = *publicKey;
    j["pks"] = *pkSig;
}

using namespace rapidjson;

ptr<NetworkMessage> NetworkMessage::parseMessage(ptr<string> _header, Schain *_sChain) {


    uint64_t sChainID;
    uint64_t blockID;
    uint64_t blockProposerIndex;
    ptr<string> type;
    uint64_t msgID;
    uint64_t srcNodeID;
    uint64_t srcSchainIndex;
    uint64_t round;
    uint64_t timeMs;
    uint8_t value;
    ptr<string> sigShare;
    ptr<string> ecdsaSig;
    ptr<string> publicKey;
    ptr<string> pkSig;

    CHECK_ARGUMENT(_header);
    CHECK_ARGUMENT(_sChain);

    try {

        auto js = nlohmann::json::parse(*_header);

        Document d;
        d.Parse(_header->data());

        CHECK_STATE(!d.HasParseError());
        CHECK_STATE(d.IsObject());
        CHECK_STATE(d.HasMember("si"));
        CHECK_STATE(d["si"].IsUint64());
        sChainID = d["si"].GetUint64();
        blockID = getUint64(js, "bi");
        blockProposerIndex = getUint64(js, "bpi");
        type = getString(js, "type");
        msgID = getUint64(js, "mi");
        srcNodeID = getUint64(js, "sni");
        srcSchainIndex = getUint64(js, "ssi");
        round = getUint64(js, "r");
        timeMs = getUint64(js, "t");
        value = getUint64(js, "v");

        if (js.find("sss") != js.end()) {
            sigShare = getString(js, "sss");
        }

        ecdsaSig = getString(js, "sig");
        publicKey = getString(js, "pk");
        pkSig = getString(js, "pks");
        CHECK_STATE(ecdsaSig);
        CHECK_STATE(publicKey);
        CHECK_STATE(pkSig);

    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(InvalidStateException("Could not parse message", __CLASS_NAME__));
    }

    try {

        if (_sChain->getSchainID() != sChainID) {
            BOOST_THROW_EXCEPTION(
                    InvalidSchainException("unknown Schain id" + to_string(sChainID), __CLASS_NAME__));
        }

        ptr<NetworkMessage> nwkMsg = nullptr;

        if (*type == BasicHeader::BV_BROADCAST) {
            nwkMsg = make_shared<BVBroadcastMessage>(node_id(srcNodeID),
                                                   block_id(blockID), schain_index(blockProposerIndex),
                                                   bin_consensus_round(round),
                                                   bin_consensus_value(value), timeMs, schain_id(sChainID), msg_id(msgID),
                                                   srcSchainIndex, ecdsaSig, publicKey, pkSig,
                                                   _sChain);
        } else if (*type == BasicHeader::AUX_BROADCAST) {
            nwkMsg = make_shared<AUXBroadcastMessage>(node_id(srcNodeID),
                                                    block_id(blockID), schain_index(blockProposerIndex),
                                                    bin_consensus_round(round),
                                                    bin_consensus_value(value),
                                                    timeMs,
                                                    schain_id(sChainID), msg_id(msgID),
                                                    sigShare,
                                                    srcSchainIndex, ecdsaSig, publicKey, pkSig,
                                                    _sChain);
        } else if (*type == BasicHeader::BLOCK_SIG_BROADCAST) {
            nwkMsg = make_shared<BlockSignBroadcastMessage>(node_id(srcNodeID),
                                                          block_id(blockID), schain_index(blockProposerIndex),
                                                          timeMs,
                                                          schain_id(sChainID), msg_id(msgID),
                                                          sigShare,
                                                          srcSchainIndex, ecdsaSig, publicKey, pkSig,
                                                          _sChain);
        } else {
            CHECK_STATE(false);
        }

        return nwkMsg;

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

schain_index NetworkMessage::getSrcSchainIndex() const {
    return srcSchainIndex;
}

uint64_t NetworkMessage::getTimeMs() const {
    return timeMs;
}

ptr<SHAHash> NetworkMessage::getHash() {
    if (hash == nullptr)
        hash = calculateHash();
    CHECK_STATE(hash);
    return hash;
}

ptr<SHAHash> NetworkMessage::calculateHash() {
    CryptoPP::SHA256 sha3;

    SHA3_UPDATE(sha3, schainID);
    SHA3_UPDATE(sha3, blockID);
    SHA3_UPDATE(sha3, blockProposerIndex);
    SHA3_UPDATE(sha3, msgID);
    SHA3_UPDATE(sha3, srcNodeID);
    SHA3_UPDATE(sha3, srcSchainIndex);
    SHA3_UPDATE(sha3, r);
    SHA3_UPDATE(sha3, value);

    CHECK_STATE(type);

    uint32_t typeLen = strlen(type);
    SHA3_UPDATE(sha3, typeLen);
    sha3.Update((unsigned char*)type, strlen(type));

    uint32_t  sigShareLen = 0;

    if (sigShareString != nullptr) {
        sigShareLen = sigShareString->size();
        SHA3_UPDATE(sha3, sigShareLen);
        sha3.Update((unsigned char *) sigShareString->data(), sigShareLen);
    } else {
        SHA3_UPDATE(sha3, sigShareLen);
    }


    auto buf = make_shared<array<uint8_t, SHA_HASH_LEN>>();
    sha3.Final(buf->data());
    hash = make_shared<SHAHash>(buf);
    return hash;
}

void NetworkMessage::sign(ptr<CryptoManager> _mgr) {
    CHECK_ARGUMENT(_mgr);
    tie(ecdsaSig, publicKey, pkSig) = _mgr->signNetworkMsg(*this);
    CHECK_STATE(ecdsaSig);
    CHECK_STATE(publicKey);
    CHECK_STATE(pkSig);
}

void NetworkMessage::verify(ptr<CryptoManager> _mgr) {
    CHECK_ARGUMENT(_mgr);
    CHECK_STATE2(_mgr->verifyNetworkMsg(*this), "ECDSA sig did not verify");
}
