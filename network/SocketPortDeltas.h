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

    @file SocketPorts.h
    @author Sidnei Teixeira
    @date 2026
*/

#pragma once

#include <cstdint>
#include "SkaleCommon.h"

/**
 * Holds the port deltas for different network sockets used by the node.
 * These deltas are added to the base port to calculate the final port numbers.
 */
struct SocketPortDeltas {
    uint16_t ZMQBinaryConsensusPortDelta = BINARY_CONSENSUS;
    uint16_t ZMQBulkDataPortDelta        = BULK_DATA_ZMQ;
    uint16_t TCPBlockProposalPortDelta   = PROPOSAL;
    uint16_t TCPCatchupPortDelta         = CATCHUP;
};
