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

class BlockFinalizeResponseHeader;
class Schain;

/**
 * Transport-neutral BlockFinalize request fields.
 *
 * Both the legacy TCP path and the new ZMQ path parse their incoming headers into this structure,
 * then hand it to BlockFinalizeResponder so the actual finalization semantics live in one place.
 */
struct BlockFinalizeRequestData {
    schain_id schainID = 0;
    block_id blockID = 0;
    schain_index proposerIndex = 0;
    fragment_index fragmentIndex = 0;
    node_id nodeID = 0;
#ifdef BITE
    epoch_id epochID = 0;
    bool needDAProofSig = false;
    bool needDecryptionShares = false;
    bool needFragmentData = false;
#endif
};

/**
 * Shared BlockFinalize server-side logic.
 *
 * This class knows how to validate a BlockFinalize request, find the proposal / committed block,
 * add DA proof and BITE share data when needed, and fill the response header + payload. It is used
 * by both CatchupServerAgent (legacy TCP) and BlockFinalizeZmqServerAgent (new persistent ZMQ lane).
 */
class BlockFinalizeResponder {
    Schain& sChain;

public:
    explicit BlockFinalizeResponder( Schain& _sChain );

    // Parse JSON request fields into a transport-neutral request object.
    static BlockFinalizeRequestData parseRequest( const nlohmann::json& _jsonRequest );

    // Apply BlockFinalize semantics and produce the binary payload, if any.
    ptr< vector< uint8_t > > createResponse( const BlockFinalizeRequestData& _request,
        const ptr< BlockFinalizeResponseHeader >& _responseHeader );
};
