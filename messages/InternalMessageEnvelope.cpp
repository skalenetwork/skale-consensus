//
// Created by stan on 01.04.18.
//

#include "../chains/Schain.h"


#include "../protocols/ProtocolKey.h"
#include "Message.h"
#include "InternalMessage.h"

#include "MessageEnvelope.h"
#include "InternalMessageEnvelope.h"


InternalMessageEnvelope::InternalMessageEnvelope(MessageOrigin origin, const ptr<Message> message, Schain& subchain,
ptr<ProtocolKey> _srcProtocolKey): MessageEnvelope(
        origin, message, subchain.getThisNodeInfo()), srcProtocolKey(_srcProtocolKey) {}
