#pragma once

#include "../protocols/ProtocolKey.h"
#include "MessageEnvelope.h"


class InternalMessageEnvelope : public MessageEnvelope {
    ptr< ProtocolKey > srcProtocolKey;

public:
    InternalMessageEnvelope( MessageOrigin origin, const ptr< Message > message, Schain& subchain,
        ptr< ProtocolKey > _srcProtocolKey = nullptr );


    const ptr< ProtocolKey >& getSrcProtocolKey() const { return srcProtocolKey; }
};
