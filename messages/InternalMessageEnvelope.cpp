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

    @file InternalMessageEnvelope.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../chains/Schain.h"


#include "../protocols/ProtocolKey.h"
#include "Message.h"
#include "InternalMessage.h"

#include "MessageEnvelope.h"
#include "InternalMessageEnvelope.h"


InternalMessageEnvelope::InternalMessageEnvelope(MessageOrigin origin, const ptr<Message> message, Schain& subchain,
ptr<ProtocolKey> _srcProtocolKey): MessageEnvelope(
        origin, message, subchain.getThisNodeInfo()), srcProtocolKey(_srcProtocolKey) {}
