/*
    Copyright (C) 2019 SKALE Labs

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

    @file ReencryptionRandomDB.h
    @author SKALE Labs
    @date 2026
*/

#ifdef BITE2

#pragma once

#include "CacheLevelDB.h"

class ReencryptionRandomDB : public CacheLevelDB {
public:
    ReencryptionRandomDB(
        Schain* _sChain, string& _dirName, string& _prefix, node_id _nodeId, uint64_t _maxDBSize );

    u256 readRandom( const block_id& _blockId );

    void writeRandom( const block_id& _blockId, const u256& _random );

    const string& getFormatVersion() override;
};

#endif