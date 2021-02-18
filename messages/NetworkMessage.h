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

#undef CHECK
#include "crypto/bls_include.h"

#include "Message.h"

#define WRITE( _BUFFER_, _VAR_ ) _BUFFER_->write( &_VAR_, sizeof( _VAR_ ) )

#define READ( _BUFFER_, _VAR_ ) _BUFFER_->read( &_VAR_, sizeof( _VAR_ ) )

class ProtocolKey;
class Schain;

class BinConsensusInstance;
class ProtocolInstance;
class Buffer;
class ConsensusBLSSigShare;
class Node;
class ThresholdSigShare;
class CryptoManager;
class ThresholdSignature;

static constexpr uint64_t MAX_CONSENSUS_MESSAGE_LEN = 1024;

#include "headers/BasicHeader.h"

class NetworkMessage : public Message, public BasicHeader {

protected:
    uint64_t timeMs = 0;
    string printPrefix = "n";
    schain_index srcSchainIndex = 0;
    bin_consensus_round r = 0;
    bin_consensus_value value = 0;
    ptr< ThresholdSigShare > sigShare;
    ptr< BLAKE3Hash > hash;
    string sigShareString;
    string ecdsaSig;
    string publicKey;
    string pkSig;

    NetworkMessage( MsgType _messageType, block_id _blockID, schain_index _blockProposerIndex,
        bin_consensus_round _r, bin_consensus_value _value, uint64_t _timeMs,
        ProtocolInstance& _srcProtocolInstance );


    NetworkMessage( MsgType _messageType, node_id _srcNodeID, block_id _blockID,
        schain_index _blockProposerIndex, bin_consensus_round _r, bin_consensus_value _value,
        uint64_t _timeMs, schain_id _schainId, msg_id _msgID, const string& _sigShareStr,
        const string& _ecdsaSig, const string& _publicKey, const string& _pkSig,
        schain_index _srcSchainIndex, const ptr< CryptoManager >& _cryptoManager );

    virtual ptr< BLAKE3Hash > calculateHash();

    void addFields( nlohmann::json& j ) override;

public:

    [[nodiscard]] uint64_t getTimeMs() const;

    void sign( const ptr< CryptoManager >& _mgr );

    void verify( const ptr< CryptoManager >& _mgr );

    [[nodiscard]] virtual bin_consensus_round getRound() const;

    [[nodiscard]] virtual bin_consensus_value getValue() const;

    void printMessage();

    [[nodiscard]] ptr< ThresholdSigShare > getSigShare() const;

    static ptr< NetworkMessage > parseMessage( const string& _header, Schain* _sChain );

    static const char* getTypeString( MsgType _type );

    [[nodiscard]] schain_index getSrcSchainIndex() const;

    ptr< BLAKE3Hash > getHash();

    string serializeToString() override;

    [[nodiscard]] const string & getECDSASig() const;
    [[nodiscard]] const string & getPublicKey() const;
    [[nodiscard]] const string & getPkSig() const;
};
