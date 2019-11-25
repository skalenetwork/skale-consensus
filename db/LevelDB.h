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

namespace leveldb {
    class DB;

    class Status;

    class Slice;
}

#define LEVELDB_PIECES 4

class LevelDB {
    vector<ptr<leveldb::DB>>db;


    uint64_t  highestDBIndex = 0;
    shared_mutex m;

    std::pair<uint64_t, uint64_t> findMaxMinDBIndex();

protected:

    node_id nodeId;
    string prefix;
    string dirname;
    uint64_t maxDBSize;

    ptr<string> readString(string &_key);


    void writeString(const string &key1, const string &value1);


    void writeByteArray(const char *_key, size_t _keyLen, const char *value,
                        size_t _valueLen);


    void writeByteArray(string &_key, const char *value,
                        size_t _valueLen);

public:


    void throwExceptionOnError(leveldb::Status result);


    LevelDB(string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize);


    class KeyVisitor {
    public:
        virtual void visitDBKey(const char *_data) = 0;
    };

    uint64_t visitKeys(KeyVisitor *_visitor, uint64_t _maxKeysToVisit);

    virtual ~LevelDB();


    uint64_t getFrontDBSize();

    bool keyExists(const char *_key);

    void rotateDBsIfNeeded();

    uint64_t getActiveDBSize();

    leveldb::DB *openDB(uint64_t _index);

    void removeDB(uint64_t index);
};


#endif //SKALED_LEVELDB_H
