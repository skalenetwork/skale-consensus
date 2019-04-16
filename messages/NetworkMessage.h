#pragma once


#include "Message.h"

#define WRITE(_BUFFER_, _VAR_) _BUFFER_->write(&_VAR_, sizeof(_VAR_))

#define READ(_BUFFER_, _VAR_) _BUFFER_->read(&_VAR_, sizeof(_VAR_))

class ProtocolKey;
class Schain;

class BinConsensusInstance;
class ProtocolInstance;
class Buffer;


class Node;



class NetworkMessage : public Message {

protected:


    string printPrefix = "n";


    NetworkMessage( MsgType _messageType, node_id _destinationNodeID, block_id blockID,
                    schain_index blockProposerIndex, bin_consensus_round _r, bin_consensus_value _value,
                    BinConsensusInstance& _srcProtocolInstance );


    NetworkMessage( MsgType messageType, node_id _srcNodeID, node_id _dstNodeID, block_id _blockID,
                    schain_index _blockProposerIndex, bin_consensus_round _r, bin_consensus_value _value,
                    schain_id _schainId, msg_id _msgID, uint32_t _ip);


public:



    bin_consensus_round r;
    bin_consensus_value value;

    bin_consensus_round getRound() const;

    bin_consensus_value getValue() const;

    msg_len getLen();

    int32_t ip;



    virtual ~NetworkMessage(){};

    void printMessage();


    int32_t getIp() const;

    void setIp(int32_t _ip);

    ptr<Buffer> toBuffer();

};
