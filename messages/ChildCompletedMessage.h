#pragma  once

#include "ChildMessage.h"

class ChildCompletedMessage : public ChildMessage {
public:
    ChildCompletedMessage(ProtocolInstance &srcProtocolInstance, ptr<ProtocolKey> key);

};



