//
// Created by stan on 19.02.18.
//
#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../protocols/ProtocolInstance.h"
#include "NetworkMessage.h"
#include "ParentMessage.h"

ParentMessage::ParentMessage( MsgType msgType, ProtocolInstance& srcProtocolInstance,
    const ptr< ProtocolKey >& dstProtocolKey )
    : InternalMessage( msgType, srcProtocolInstance, dstProtocolKey ) {}
