//
// Created by stan on 01.04.18.
//
#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "../protocols/ProtocolKey.h"
#include "../node/NodeInfo.h"
#include "../messages/Message.h"
#include "MessageEnvelope.h"



const ptr<Message> MessageEnvelope::getMessage() const {
    return message;
}


MessageOrigin MessageEnvelope::getOrigin() const {
    return origin;
}

MessageEnvelope::MessageEnvelope(MessageOrigin origin, const ptr<Message> &message,
                                 const ptr<NodeInfo> &realSender) : origin(origin), message(message),
                                                                                srcNodeInfo(realSender) {}


