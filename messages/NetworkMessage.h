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

    @file NetworkMessage.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#include "crypto/bls_include.h"

#include "Message.h"

#define WRITE(_BUFFER_, _VAR_) _BUFFER_->write(&_VAR_, sizeof(_VAR_))

#define READ(_BUFFER_, _VAR_) _BUFFER_->read(&_VAR_, sizeof(_VAR_))

class ProtocolKey;
class Schain;

class BinConsensusInstance;
class ProtocolInstance;
class Buffer;
class ConsensusBLSSigShare;
class Node;
class ThresholdSigShare;
class CryptoManager;

static constexpr uint64_t MAX_CONSENSUS_MESSAGE_LEN = 1024;

#include "headers/BasicHeader.h"

class NetworkMessage : public Message, public BasicHeader {


protected:

    void addFields(nlohmann::json &j) override;

    string printPrefix = "n";

    ptr<string> sigShareString;

    ptr<ThresholdSigShare> sigShare;

    NetworkMessage(MsgType _messageType, block_id _blockID, schain_index _blockProposerIndex,
                   bin_consensus_round _r, bin_consensus_value _value,
                   ProtocolInstance &_srcProtocolInstance);


    NetworkMessage(MsgType _messageType, node_id _srcNodeID, block_id _blockID,
                   schain_index _blockProposerIndex, bin_consensus_round _r, bin_consensus_value _value,
                   schain_id _schainId, msg_id _msgID, ptr<string> _sigShareStr,
                   schain_index _srcSchainIndex, ptr<CryptoManager> _cryptoManager);

    schain_index srcSchainIndex;
    bin_consensus_round r;
    bin_consensus_value value;

    ptr<SHAHash> hash;

    virtual ptr<SHAHash> calculateHash();


public:

    virtual bin_consensus_round getRound() const;

    virtual bin_consensus_value getValue() const;

    virtual ~NetworkMessage(){};

    void printMessage();

    void setIp(int32_t _ip);

    ptr<ThresholdSigShare> getSigShare() const;

    static ptr<NetworkMessage> parseMessage(ptr<string> _header, Schain* _sChain);

    static const char* getTypeString(MsgType _type );

    schain_index getSrcSchainIndex() const;


    ptr<SHAHash> getHash();

};
