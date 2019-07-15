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

    @file RandomDB.cpp
    @author Stan Kladko
    @date 2019
*/

#include "../SkaleCommon.h"
#include "../Log.h"


#include "RandomDB.h"




RandomDB::RandomDB(string& filename, node_id _nodeId ) : LevelDB( filename, _nodeId ) {}


const string RandomDB::getFormatVersion() {
    return "1.0";
}




ptr<string>
RandomDB::readRandom( schain_id _sChainID, const block_id& _blockId,
    const schain_index& _proposerIndex, const bin_consensus_round& _round) {



        string keyStr;

        stringstream key;


        keyStr = getKey( _sChainID, _blockId, _proposerIndex, _round, keyStr, key );


        return readString(keyStr);

}


void
RandomDB::writeRandom( schain_id _sChainID, const block_id& _blockId,
                      const schain_index& _proposerIndex, const bin_consensus_round& _round, uint64_t _random) {



    string keyStr;

    stringstream key;


    keyStr = getKey( _sChainID, _blockId, _proposerIndex, _round, keyStr, key );


    writeString(keyStr, to_string(_random));

}

string & RandomDB::getKey(const schain_id& _sChainID, const block_id& _blockId, const schain_index& _proposerIndex, const bin_consensus_round& _round, string& keyStr, stringstream& key) const {
    key << _sChainID << ":" << _blockId << ":" << _proposerIndex << ":" << _round;

    keyStr = key.str();
    return keyStr;
}