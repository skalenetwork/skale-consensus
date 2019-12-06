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

    @file LevelDB.h
    @author Stan Kladko
    @date 2019
*/


#ifndef SKALED_LEVELDB_H
#define SKALED_LEVELDB_H

#include "../SkaleCommon.h"

namespace leveldb {
    class DB;

    class Status;

    class Slice;
}

#define LEVELDB_PIECES 4

namespace cache {
    template<typename key_t, typename value_t> class lru_cache;
}


class LevelDB {
    vector<ptr<leveldb::DB>>db;
    uint64_t  highestDBIndex = 0;
    shared_mutex m;




protected:

    uint64_t totalSigners;
    uint64_t requiredSigners;


    node_id nodeId;
    string prefix;
    string dirname;
    uint64_t maxDBSize;

    ptr<string> readString(string &_key);
    ptr<string> readStringUnsafe(string &_key);
    void writeString(const string &key1, const string &value1);

    ptr<map<schain_index, ptr<string>>>
    writeStringToSet(const string &_value, block_id _blockId, schain_index _index);


        ptr<map<schain_index, ptr<string>>>
    writeByteArrayToSet(const char *_value, uint64_t _valueLen, block_id _blockId, schain_index _index);

    void writeByteArray(const char *_key, size_t _keyLen, const char *value,
                        size_t _valueLen);
    void writeByteArray(string &_key, const char *value,
                        size_t _valueLen);


    ptr<string> createKey(block_id _blockId);

    ptr<string> createKey(block_id _blockId, schain_index _proposerIndex);


    ptr<string>
    createKey(const block_id &_blockId, const schain_index &_proposerIndex, const bin_consensus_round &_round);

    string createSetKey(block_id _blockId, schain_index _index);

    string createCounterKey(block_id _block_id);


public:

    virtual const string getFormatVersion() = 0;

    void throwExceptionOnError(leveldb::Status result);


    LevelDB(string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize,
            uint64_t _totalSigners = 0, uint64_t _requiredSigners = 0);


    std::pair<uint64_t, uint64_t> findMaxMinDBIndex();


    class KeyVisitor {
    public:
        virtual void visitDBKey(const char *_data) = 0;
    };

    uint64_t visitKeys(KeyVisitor *_visitor, uint64_t _maxKeysToVisit);

    virtual ~LevelDB();


    void rotateDBsIfNeeded();

    uint64_t getActiveDBSize();

    leveldb::DB *openDB(uint64_t _index);

    bool keyExists(const string &_key);

    bool keyExistsInSet(block_id _blockId, schain_index _index);

    ptr<string> readStringFromBlockSet(block_id _blockId, schain_index _index);

    uint64_t readCount(block_id _blockId);

    bool isEnough(block_id _blockID);
};


#endif //SKALED_LEVELDB_H
