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
*/

#pragma once

#include "SkaleCommon.h"
#include "thirdparty/json.hpp"

class Header;
class Schain;

/**
 * Helper for ZMQ request/reply protocols that use a small structured header
 * plus an optional binary payload.
 *
 * This is intended for generic use (e.g. bulk-data or recovery-style traffic such as
 * BlockFinalize, catchup, or similar non-hot-path requests). It is not meant
 * for the latency-sensitive consensus message path, which uses its own more
 * compact message format.
 *
 * Wire format:
 * [uint64 header_len][serialized JSON header][optional payload bytes]
 */
class ZMQHeaderPayloadFrame {
public:
    // Serialize a completed Header plus optional binary body into one ZMQ frame.
    static ptr< vector< uint8_t > > packMessage(
        const ptr< Header >& _header, const ptr< vector< uint8_t > >& _payload = nullptr );

    // Parse one ZMQ frame back into JSON header + optional binary body.
    static nlohmann::json unpackMessage( const void* _data, size_t _len,
        ptr< vector< uint8_t > >& _payload, const char* _errorString,
        uint64_t _maxHeaderLen = MAX_HEADER_SIZE );

    // Send a frame on a client socket.
    static void sendFrame( Schain& _sChain, void* _socket, const ptr< vector< uint8_t > >& _frame );

    // Send a frame from a server socket back to the request's routing id.
    static void sendFrameToRoutingId( Schain& _sChain, void* _socket, uint32_t _routingId,
        const ptr< vector< uint8_t > >& _frame );

    // Receive one frame. Server-side loops can opt out of throwing on idle timeout.
    static ptr< vector< uint8_t > > receiveFrame(
        Schain& _sChain, void* _socket, uint32_t* _routingId, const char* _errorString,
        bool _throwOnTimeout = true );
};
