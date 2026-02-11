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

    @file ReencryptionRandomDB.cpp
    @author SKALE Labs
    @date 2026
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/ExitRequestedException.h"
#include "LevelDBOptions.h"
#include "ReencryptionRandomDB.h"

ReencryptionRandomDB::ReencryptionRandomDB(
    Schain* _sChain, string& _dirName, string& _prefix, node_id _nodeId, uint64_t _maxDBSize )
    : CacheLevelDB( _sChain, _dirName, _prefix, _nodeId, _maxDBSize,
          LevelDBOptions::getReencryptionRandomDBOptions(), false ) {}


const string& ReencryptionRandomDB::getFormatVersion() {
    static const string version = "1.0";
    return version;
}


u256 ReencryptionRandomDB::readRandom( const block_id& _blockId ) {
    try {
        auto key = createKey( _blockId );
        CHECK_STATE( !key.empty() )
        auto value = readString( key );
        CHECK_STATE( !value.empty() )
        return u256( value.c_str() );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void ReencryptionRandomDB::writeRandom( const block_id& _blockId, const u256& _random ) {
    try {
        auto key = createKey( _blockId );
        CHECK_STATE( !key.empty() )
        writeString( key, _random.str() );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}
