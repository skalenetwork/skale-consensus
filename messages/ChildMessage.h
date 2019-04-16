#pragma once

#include "InternalMessage.h"

class ProtocolInstance;


class ChildMessage : public InternalMessage {


public:
    ChildMessage(MsgType msgType, ProtocolInstance &srcProtocolInstance, ptr<ProtocolKey> key);

};



