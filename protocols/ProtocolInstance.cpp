#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../thirdparty/json.hpp"
#include "../abstracttcpserver/ConnectionStatus.h"
#include "../messages/ParentMessage.h"
#include "../messages/ChildCompletedMessage.h"
#include "../pendingqueue/PendingTransactionsAgent.h"
#include "../blockproposal/pusher/BlockProposalClientAgent.h"
#include "../blockproposal/received/ReceivedBlockProposalsDatabase.h"
#include "../chains/Schain.h"
#include "../protocols/ProtocolKey.h"
#include "../protocols/binconsensus/BinConsensusInstance.h"


msg_id ProtocolInstance::createNetworkMessageID() {
    this->messageCounter+=1;
    return messageCounter;
}


ProtocolInstance::ProtocolInstance(
    ProtocolType _protocolType // unused
    , Schain& _sChain
    )
    : sChain(_sChain)
    , protocolType(_protocolType) // unused
    , messageCounter(0)
{
    totalObjects++;
}


Schain *ProtocolInstance::getSchain() const {
    return &sChain;
}


void ProtocolInstance::setStatus(ProtocolStatus _status) {
    ProtocolInstance::status = _status;
}

void ProtocolInstance::setOutcome(ProtocolOutcome outcome) {
    ProtocolInstance::outcome = outcome;
}




ProtocolOutcome ProtocolInstance::getOutcome() const {
    return outcome;
}

ProtocolInstance::~ProtocolInstance() {
    totalObjects--;
}


atomic<uint64_t>  ProtocolInstance::totalObjects(0);






