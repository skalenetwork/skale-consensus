#pragma  once


#include "InternalMessage.h"

class ProtocolInstance;


class ParentMessage : public InternalMessage {
public:
    ParentMessage(MsgType msgType, ProtocolInstance &srcProtocolInstance,
                  const ptr<ProtocolKey> &dstProtocolKey);
};
