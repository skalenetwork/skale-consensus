//
// Created by stan on 01.04.18.
//

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "ParentCompletedMessage.h"

ParentCompletedMessage::ParentCompletedMessage(ProtocolInstance &srcProtocolInstance,
                                               const ptr<ProtocolKey> &dstProtocolKey) : ParentMessage(PARENT_COMPLETED,
                                                                                                srcProtocolInstance,
                                                                                                dstProtocolKey) {}
