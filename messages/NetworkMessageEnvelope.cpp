/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file NetworkMessageEnvelope.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "SkaleLog.h"

#include "NetworkMessage.h"

#include "exceptions/FatalError.h"
#include "node/Node.h"
#include "thirdparty/json.hpp"

#include "node/NodeInfo.h"
#include "utils/Time.h"
#include "MessageEnvelope.h"
#include "NetworkMessageEnvelope.h"


NetworkMessageEnvelope::NetworkMessageEnvelope(
    const ptr< NetworkMessage >& message, const ptr< NodeInfo >& realSender )
    : MessageEnvelope( ORIGIN_NETWORK, message, realSender ) {
    arrivalTime = Time::getCurrentTimeMs();
}

uint64_t NetworkMessageEnvelope::getArrivalTime() const {
    return arrivalTime;
}
