//
// Created by stan on 01.04.18.
//

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "../protocols/ProtocolKey.h"
#include "../protocols/ProtocolInstance.h"
#include "ChildCompletedMessage.h"

using namespace std;

ChildCompletedMessage::ChildCompletedMessage(ProtocolInstance &srcProtocolInstance,  ptr<ProtocolKey> key) : ChildMessage(CHILD_COMPLETED,
                                                                                                           srcProtocolInstance, key) {}
