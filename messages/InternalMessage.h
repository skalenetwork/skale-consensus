#pragma once
#include <memory>
#include "Message.h"


class ProtocolInstance;
class ProtocolKey;




class InternalMessage : public Message {

public:

    InternalMessage(MsgType msgType, ProtocolInstance &srcProtocolInstance, const ptr<ProtocolKey> &protocolKey);




};
