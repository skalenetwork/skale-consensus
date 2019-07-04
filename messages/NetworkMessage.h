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



class NetworkMessage : public Message {

protected:


    string printPrefix = "n";


    NetworkMessage( MsgType _messageType, node_id _destinationNodeID, block_id blockID,
                    schain_index blockProposerIndex, bin_consensus_round _r, bin_consensus_value _value,
                    BinConsensusInstance& _srcProtocolInstance );


    NetworkMessage(MsgType messageType, node_id _srcNodeID, node_id _dstNodeID, block_id _blockID,
                   schain_index _blockProposerIndex, bin_consensus_round _r, bin_consensus_value _value,
                   schain_id _schainId, msg_id _msgID, uint32_t _ip, ptr<string> _signature,
                   schain_index _srcSchainIndex);


public:



    bin_consensus_round r;
    bin_consensus_value value;

    bin_consensus_round getRound() const;

    bin_consensus_value getValue() const;

    msg_len getLen();

    int32_t ip;

    ptr<string> sigShareString;

    ptr<ConsensusBLSSigShare> sigShare;


    virtual ~NetworkMessage(){};

    void printMessage();


    int32_t getIp() const;

    void setIp(int32_t _ip);

    ptr<Buffer> toBuffer();

    ptr<ConsensusBLSSigShare> getSigShare() const;

};
