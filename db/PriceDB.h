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

    @file PriceDB.h
    @author Stan Kladko
    @date 2019
*/


#ifndef SKALED_PRICEDB_H
#define SKALED_PRICEDB_H


#include "CacheLevelDB.h"

class PriceDB : public CacheLevelDB {
public:
    const string& getFormatVersion() override;

    PriceDB(
        Schain* _sChain, string& _dirName, string& _prefix, node_id _nodeId, uint64_t _maxDBSize );

    u256 readPrice( block_id _blockID );

    void savePrice( const u256& _price, block_id _blockID );
};


#endif
