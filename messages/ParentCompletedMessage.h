#pragma once

#include "ParentMessage.h"

class ParentCompletedMessage : public ParentMessage {

public:
    ParentCompletedMessage(ProtocolInstance &srcProtocolInstance,
                           const ptr<ProtocolKey> &dstProtocolKey
                         );
};




