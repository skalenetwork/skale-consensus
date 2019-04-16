

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../thirdparty/json.hpp"
#include "../exceptions/FatalError.h"
#include "../chains/Schain.h"
#include "../node/Node.h"
#include "../protocols/ProtocolKey.h"
#include "../protocols/binconsensus/BinConsensusInstance.h"
#include "../protocols/blockconsensus/BlockConsensusAgent.h"
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
