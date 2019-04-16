//
// Created by skale on 4/6/19.
//

#ifndef SKALED_LEVELDB_H
#define SKALED_LEVELDB_H

namespace leveldb {
    class DB;
    class Status;
    class Slice;
}

class LevelDB {

    leveldb::DB* db;

public:

    LevelDB(string& filename);


    ptr<string> readString(string& _key);

    void writeString(const string &key1, const string &value1);

    void writeByteArray(const char *_key, size_t _keyLen, const char *value,
                        size_t _valueLen);


    void writeByteArray(string& _key, const char *value,
                                 size_t _valueLen);

    void throwExceptionOnError(leveldb::Status result);

    class KeyVisitor {
      public:
        virtual void visitDBKey(leveldb::Slice key) = 0;
    };

    uint64_t visitKeys(KeyVisitor* _visitor, uint64_t _maxKeysToVisit);

    virtual ~LevelDB();

};


#endif //SKALED_LEVELDB_H
