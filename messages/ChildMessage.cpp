//
// Created by stan on 01.04.18.
//

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../protocols/ProtocolKey.h"
#include "../protocols/ProtocolInstance.h"
#include "ChildMessage.h"

ChildMessage::ChildMessage(MsgType msgType, ProtocolInstance &srcProtocolInstance,
ptr<ProtocolKey> key) : InternalMessage(msgType,srcProtocolInstance,  key) {}
