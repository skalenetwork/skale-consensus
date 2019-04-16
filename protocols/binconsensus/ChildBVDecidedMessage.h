#pragma once




#include "../../messages/ChildMessage.h"

class ChildBVDecidedMessage : public ChildMessage {

    bool value;
public:
    bool getValue() const;

public:
    ChildBVDecidedMessage(bool value, ProtocolInstance &srcProtocolInstance, ptr<ProtocolKey> key);

};