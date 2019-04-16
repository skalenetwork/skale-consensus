#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../thirdparty/json.hpp"
#include "NetworkMessage.h"
#include "../node/Node.h"
#include "../node/NodeInfo.h"
#include "MessageEnvelope.h"
#include "NetworkMessageEnvelope.h"


NetworkMessageEnvelope::NetworkMessageEnvelope(
    const ptr< NetworkMessage >& message, const ptr< NodeInfo >& realSender )
    : MessageEnvelope( ORIGIN_NETWORK, message, realSender ) {}
