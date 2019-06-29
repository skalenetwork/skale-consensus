/*
    Copyright (C) 2019 SKALE Labs

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

    @file HistoryMessage.h
    @author Stan Kladko
    @date 2019
*/

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
