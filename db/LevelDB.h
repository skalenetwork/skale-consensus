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

class LevelDB {

    leveldb::DB* db;

protected:

    node_id nodeId;



    ptr<string> readString(string& _key);


    void writeString(const string &key1, const string &value1);



    void writeByteArray(const char *_key, size_t _keyLen, const char *value,
                        size_t _valueLen);




    void writeByteArray(string& _key, const char *value,
                        size_t _valueLen);

public:


    void throwExceptionOnError(leveldb::Status result);


    LevelDB(string& filename,  node_id _nodeId);




    class KeyVisitor {
      public:
        virtual void visitDBKey(leveldb::Slice key) = 0;
    };

    uint64_t visitKeys(KeyVisitor* _visitor, uint64_t _maxKeysToVisit);

    virtual ~LevelDB();


    ptr<string>  createKey(const block_id _blockId);


};


#endif //SKALED_LEVELDB_H
