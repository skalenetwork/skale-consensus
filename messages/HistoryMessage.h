#pragma once

#include "NetworkMessage.h"

class HistoryMessage : public NetworkMessage {

protected:

    HistoryMessage(MsgType _messageType, bin_consensus_round _r, bin_consensus_value _value,
                   BinConsensusInstance &_srcProtocolInstance);

};


class HistoryDecideMessage : public HistoryMessage {

public:

    HistoryDecideMessage(bin_consensus_round _r, bin_consensus_value _value, BinConsensusInstance &_srcProtocolInstance);

};



class HistoryBVSelfVoteMessage : public HistoryMessage {

public:

    HistoryBVSelfVoteMessage(bin_consensus_round _r, bin_consensus_value _value, BinConsensusInstance &_srcProtocolInstance);

};


class HistoryAUXSelfVoteMessage : public HistoryMessage {

public:

    HistoryAUXSelfVoteMessage(bin_consensus_round _r, bin_consensus_value _value, BinConsensusInstance &_srcProtocolInstance);

};


class HistoryCommonCoinMessage : public HistoryMessage {

public:

    HistoryCommonCoinMessage(bin_consensus_round _r, bin_consensus_value _value, BinConsensusInstance &_srcProtocolInstance);

};




class HistoryNewRoundMessage : public HistoryMessage {

public:

    HistoryNewRoundMessage(bin_consensus_round _r, bin_consensus_value _value, BinConsensusInstance &_srcProtocolInstance);

};
