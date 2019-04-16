#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../thirdparty/json.hpp"

#include "../chains/Schain.h"
#include "../protocols/ProtocolInstance.h"
#include "../protocols/ProtocolKey.h"
#include "../node/Node.h"
#include "InternalMessage.h"

using namespace std;


InternalMessage::InternalMessage(MsgType msgType,
                                 ProtocolInstance &srcProtocolInstance,
                                 const ptr<ProtocolKey> &protocolKey) :

    Message(srcProtocolInstance.getSchain()->getSchainID(),
            msgType, srcProtocolInstance.createNetworkMessageID(), srcProtocolInstance.getSchain()->getNode()->getNodeID(),
            srcProtocolInstance.getSchain()->getNode()->getNodeID(),  protocolKey->getBlockID(),
            protocolKey->getBlockProposerIndex()) {
    ASSERT(protocolKey != nullptr);
    ASSERT((uint64_t)protocolKey->getBlockID() != 0);

}
