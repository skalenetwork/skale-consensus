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

#include "thirdparty/rapidjson/document.h"
#include "thirdparty/json.hpp"
#include "thirdparty/rapidjson/prettywriter.h" // for stringify JSON

#include "SkaleCommon.h"
#include "Log.h"

#include "Message.h"

#include "NetworkMessageEnvelope.h"

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
#include "oracle/OracleRequestBroadcastMessage.h"
#include "protocols/ProtocolKey.h"
#include "protocols/binconsensus/AUXBroadcastMessage.h"
#include "protocols/binconsensus/BVBroadcastMessage.h"
#include "protocols/binconsensus/BinConsensusInstance.h"
#include "protocols/blockconsensus/BlockSignBroadcastMessage.h"


#include <crypto/BLAKE3Hash.h>


#include "NetworkMessage.h"


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
                               schain_id _schainId, msg_id _msgID, const string& _sigShareStr, const string& _ecdsaSig,
                               const string& _publicKey, const string& _pkSig,
                               schain_index _srcSchainIndex, const ptr<CryptoManager>& _cryptoManager)
        : Message(_schainId, _messageType, _msgID, _srcNodeID, _blockID, _blockProposerIndex),
          BasicHeader(getTypeString(_messageType)) {

    CHECK_ARGUMENT(_srcSchainIndex > 0)
    CHECK_ARGUMENT(!_ecdsaSig.empty())
    CHECK_ARGUMENT(_cryptoManager)
    CHECK_ARGUMENT(_timeMs > 0)

    this->srcSchainIndex = _srcSchainIndex;
    this->r = _r;
    this->value = _value;
    this->timeMs = _timeMs;
    this->sigShareString = _sigShareStr;
    this->ecdsaSig = _ecdsaSig;
    this->publicKey = _publicKey;
    this->pkSig = _pkSig;

    if (!_sigShareStr.empty()) {
        sigShare = _cryptoManager->createSigShare(_sigShareStr, _schainId, _blockID, _srcSchainIndex,
                                                  (uint64_t) _r <= 3);
        CHECK_STATE(sigShare)
    }

    setComplete();

}
const string& NetworkMessage::getPublicKey() const {
    return publicKey;
}
const string& NetworkMessage::getPkSig() const {
    return pkSig;
}

ptr<ThresholdSigShare> NetworkMessage::getSigShare() const {
    CHECK_STATE(sigShare);
    return sigShare;
}

const string &NetworkMessage::getECDSASig() const {
    CHECK_STATE(!ecdsaSig.empty())
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


using namespace rapidjson;

// fast serialization for minimum info
string NetworkMessage::serializeToStringLite() {
    CHECK_STATE(complete);


    StringBuffer sb;
    Writer<StringBuffer> writer(sb);

    writer.StartObject();

    CHECK_STATE(type != nullptr);

    writer.String("type");
    writer.String(type);
    writer.String("bpi");
    writer.Uint64((uint64_t )getBlockProposerIndex());
    writer.String("mt");
    writer.Uint64((uint64_t )msgType);
    writer.String("mi");
    writer.Uint64((uint64_t )msgID);
    writer.String("sni");
    writer.Uint64((uint64_t) srcNodeID);
    writer.String("ssi");
    writer.Uint64((uint64_t) srcSchainIndex);
    writer.String("r");
    writer.Uint64((uint64_t )r);
    writer.String("t");
    writer.Uint64((uint64_t) timeMs);
    writer.String("v");
    writer.Uint64((uint8_t )value);

    if (!sigShareString.empty()) {
        writer.String("sss");
        writer.String(sigShareString.data(), sigShareString.size());
    }

    CHECK_STATE(!ecdsaSig.empty())
    writer.String("sig");
    writer.String(ecdsaSig.data(), ecdsaSig.size());
    writer.String("pk");
    writer.String(publicKey.data(), publicKey.size());
    writer.String("pks");
    writer.String(pkSig.data(), pkSig.size());

    writer.EndObject();
    writer.Flush();
    string s(sb.GetString());
    return s;

}

string NetworkMessage::serializeToString() {
    CHECK_STATE(complete);


    StringBuffer sb;
    Writer<StringBuffer> writer(sb);

    writer.StartObject();

    CHECK_STATE(type != nullptr);

    writer.String("type");
    writer.String(type);
    writer.String("si");
    writer.Uint64((uint64_t ) schainID);
    writer.String("bi");
    writer.Uint64((uint64_t )blockID);
    writer.String("bpi");
    writer.Uint64((uint64_t )getBlockProposerIndex());
    writer.String("mt");
    writer.Uint64((uint64_t )msgType);
    writer.String("mi");
    writer.Uint64((uint64_t )msgID);
    writer.String("sni");
    writer.Uint64((uint64_t) srcNodeID);
    writer.String("ssi");
    writer.Uint64((uint64_t) srcSchainIndex);
    writer.String("r");
    writer.Uint64((uint64_t )r);
    writer.String("t");
    writer.Uint64((uint64_t) timeMs);
    writer.String("v");
    writer.Uint64((uint8_t )value);

    if (!sigShareString.empty()) {
        writer.String("sss");
        writer.String(sigShareString.data(), sigShareString.size());
    }

    CHECK_STATE(!ecdsaSig.empty())
    writer.String("sig");
    writer.String(ecdsaSig.data(), ecdsaSig.size());
    writer.String("pk");
    writer.String(publicKey.data(), publicKey.size());
    writer.String("pks");
    writer.String(pkSig.data(), pkSig.size());

    writer.EndObject();
    writer.Flush();
    string s(sb.GetString());

    CHECK_STATE(s.size() > 16);

    return s;

}

void NetworkMessage::addFields(nlohmann::basic_json<>& ) {

    /*
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
     */
    CHECK_STATE(false); // not used anymore
}


ptr<NetworkMessage> NetworkMessage::parseMessage(const string& _header, Schain *_sChain, bool _lite) {


    uint64_t sChainID;
    uint64_t blockID;
    uint64_t blockProposerIndex;
    string type;
    uint64_t msgID;
    uint64_t srcNodeID;
    uint64_t srcSchainIndex;
    uint64_t round;
    uint64_t timeMs;
    uint8_t value;
    string sigShare;
    string ecdsaSig;
    string publicKey;
    string pkSig;

    CHECK_ARGUMENT(!_header.empty());
    CHECK_ARGUMENT(_sChain);

    Document d;

    try {


        d.Parse(_header.data());

        CHECK_STATE(!d.HasParseError());
        CHECK_STATE(d.IsObject())
        if (_lite) {
            sChainID = (uint64_t ) _sChain->getSchainID();
            blockID = (uint64_t ) _sChain->getLastCommittedBlockID() + 1;
        } else {
            sChainID = getUint64Rapid(d, "si");
            blockID = getUint64Rapid(d, "bi");
        }
        blockProposerIndex = getUint64Rapid(d, "bpi");
        type = getStringRapid(d, "type");
        msgID = getUint64Rapid(d, "mi");
        srcNodeID = getUint64Rapid(d, "sni");
        srcSchainIndex = getUint64Rapid(d, "ssi");
        round = getUint64Rapid(d, "r");
        timeMs = getUint64Rapid(d, "t");
        value = getUint64Rapid(d, "v");

        if (d.HasMember("sss")) {
            sigShare = getStringRapid(d, "sss");
        }

        ecdsaSig = getStringRapid(d, "sig");
        publicKey = getStringRapid(d, "pk");
        pkSig = getStringRapid(d, "pks");
        CHECK_STATE(!ecdsaSig.empty())

    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(InvalidStateException("Could not parse message", __CLASS_NAME__));
    }

    try {

        if (_sChain->getSchainID() != sChainID) {
            BOOST_THROW_EXCEPTION(
                    InvalidSchainException("unknown Schain id" + to_string(sChainID), __CLASS_NAME__));
        }

        ptr<NetworkMessage> nwkMsg = nullptr;

        if (type == BasicHeader::BV_BROADCAST) {
            nwkMsg = make_shared<BVBroadcastMessage>(node_id(srcNodeID),
                                                   block_id(blockID), schain_index(blockProposerIndex),
                                                   bin_consensus_round(round),
                                                   bin_consensus_value(value), timeMs, schain_id(sChainID), msg_id(msgID),
                                                   srcSchainIndex, ecdsaSig, publicKey, pkSig,
                                                   _sChain);
        } else if (type == BasicHeader::AUX_BROADCAST) {
            nwkMsg = make_shared<AUXBroadcastMessage>(node_id(srcNodeID),
                                                    block_id(blockID), schain_index(blockProposerIndex),
                                                    bin_consensus_round(round),
                                                    bin_consensus_value(value),
                                                    timeMs,
                                                    schain_id(sChainID), msg_id(msgID),
                                                    sigShare,
                                                    srcSchainIndex, ecdsaSig, publicKey, pkSig,
                                                    _sChain);
        } else if (type == BasicHeader::BLOCK_SIG_BROADCAST) {
            nwkMsg = make_shared<BlockSignBroadcastMessage>(node_id(srcNodeID),
                                                          block_id(blockID), schain_index(blockProposerIndex),
                                                          timeMs,
                                                          schain_id(sChainID), msg_id(msgID),
                                                          sigShare,
                                                          srcSchainIndex, ecdsaSig, publicKey, pkSig,
                                                          _sChain);
        } else if (type == BasicHeader::ORACLE_REQUEST_BROADCAST) {
            string uri = getStringRapid(d, "uri");
            CHECK_STATE(!uri.empty())
            nwkMsg = make_shared<OracleRequestBroadcastMessage>(uri, node_id(srcNodeID),
                                                            block_id(blockID),
                                                            timeMs,
                                                            schain_id(sChainID), msg_id(msgID),
                                                            srcSchainIndex, ecdsaSig, publicKey, pkSig,
                                                            _sChain);
        } else {
            CHECK_STATE(false)
        }


        return nwkMsg;

    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(InvalidStateException("Could not create message of type:"
                                                + type , __CLASS_NAME__));
    }
}


const char *NetworkMessage::getTypeString(MsgType _type) {
    switch (_type) {
        case MSG_BVB_BROADCAST : {
            return BV_BROADCAST;
        }
        case MSG_AUX_BROADCAST : {
            return AUX_BROADCAST;
        }
        case MSG_BLOCK_SIGN_BROADCAST : {
            return BLOCK_SIG_BROADCAST;
        }
        case MSG_ORACLE_REQ_BROADCAST : {
            return ORACLE_REQUEST_BROADCAST;
        }
        default: {
            return "history";
        }
    }
}

schain_index NetworkMessage::getSrcSchainIndex() const {
    return srcSchainIndex;
}

uint64_t NetworkMessage::getTimeMs() const {
    return timeMs;
}

BLAKE3Hash NetworkMessage::getHash() {
    if (!haveHash) {
        hash = calculateHash();
        haveHash = true;
    }

    return hash;
}

void NetworkMessage::updateWithChildHash(blake3_hasher& ) {

}

BLAKE3Hash NetworkMessage::calculateHash() {

    HASH_INIT(hasher);
    HASH_UPDATE(hasher, schainID);
    HASH_UPDATE(hasher, blockID);
    HASH_UPDATE(hasher, blockProposerIndex);
    HASH_UPDATE(hasher, msgID);
    HASH_UPDATE(hasher, srcNodeID);
    HASH_UPDATE(hasher, srcSchainIndex);
    HASH_UPDATE(hasher, r);
    HASH_UPDATE(hasher, value);

    CHECK_STATE(type)

    uint32_t typeLen = strlen(type);
    HASH_UPDATE(hasher, typeLen);
    blake3_hasher_update(&hasher, (unsigned char*)type, strlen(type));

    uint32_t  sigShareLen = sigShareString.size();
    HASH_UPDATE(hasher, sigShareLen);
    if (sigShareLen > 0) {
        blake3_hasher_update(&hasher, (unsigned char *) sigShareString.data(), sigShareLen);
    }

    HASH_FINAL(hasher, hash.data());
    return hash;
}

void NetworkMessage::sign(const ptr<CryptoManager>& _mgr) {
    CHECK_ARGUMENT(_mgr)
    tie(ecdsaSig, publicKey, pkSig) = _mgr->signNetworkMsg(*this);
    CHECK_STATE(!ecdsaSig.empty())
}

void NetworkMessage::verify(const ptr<CryptoManager>& _mgr) {
    CHECK_ARGUMENT(_mgr)
    CHECK_STATE2(_mgr->verifyNetworkMsg(*this), "ECDSA sig did not verify")
}
