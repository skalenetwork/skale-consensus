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

    @file HistoryMessage.cpp
    @author Stan Kladko
    @date 2019
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "thirdparty/json.hpp"
#include "crypto/bls_include.h"
#include "exceptions/FatalError.h"
#include "chains/Schain.h"
#include "node/Node.h"
#include "protocols/ProtocolKey.h"
#include "protocols/binconsensus/BinConsensusInstance.h"
#include "protocols/blockconsensus/BlockConsensusAgent.h"
#include "HistoryMessage.h"

HistoryMessage::HistoryMessage(MsgType _messageType, bin_consensus_round _r, bin_consensus_value _value,
                               BinConsensusInstance &_srcProtocolInstance)
        : NetworkMessage(_messageType,
                         _srcProtocolInstance.getBlockConsensusInstance()->getSchain()->getNode()->getNodeID(), _srcProtocolInstance.getBlockID(),
                         _srcProtocolInstance.getBlockProposerIndex(), _r, _value,
                         _srcProtocolInstance) {
}



HistoryDecideMessage::HistoryDecideMessage(bin_consensus_round _r, bin_consensus_value _value,
                                           BinConsensusInstance &_srcProtocolInstance) : HistoryMessage(
        MsgType::BIN_CONSENSUS_HISTORY_DECIDE, _r, _value, _srcProtocolInstance) {
    printPrefix = "d";

}



HistoryBVSelfVoteMessage::HistoryBVSelfVoteMessage(bin_consensus_round _r, bin_consensus_value _value,
                                                     BinConsensusInstance &_srcProtocolInstance) : HistoryMessage(
        MsgType::BIN_CONSENSUS_HISTORY_BVSELF, _r, _value, _srcProtocolInstance) {
    printPrefix = "bs";

}

HistoryAUXSelfVoteMessage::HistoryAUXSelfVoteMessage(bin_consensus_round _r, bin_consensus_value _value,
                                                     BinConsensusInstance &_srcProtocolInstance) : HistoryMessage(
        MsgType::BIN_CONSENSUS_HISTORY_AUXSELF, _r, _value, _srcProtocolInstance) {
    printPrefix = "as";

}


HistoryCommonCoinMessage::HistoryCommonCoinMessage(bin_consensus_round _r, bin_consensus_value _value,
                                                   BinConsensusInstance &_srcProtocolInstance) : HistoryMessage(
        MsgType::BIN_CONSENSUS_HISTORY_CC, _r, _value, _srcProtocolInstance) {
    printPrefix = "cc";

}


HistoryNewRoundMessage::HistoryNewRoundMessage(bin_consensus_round _r, bin_consensus_value _value,
                                               BinConsensusInstance &_srcProtocolInstance) : HistoryMessage(
        MsgType::BIN_CONSENSUS_HISTORY_NEW_ROUND, _r, _value, _srcProtocolInstance) {
    printPrefix = "nr";

}
