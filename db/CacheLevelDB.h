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


#ifndef SKALED_CACHELEVELDB_H
#define SKALED_CACHELEVELDB_H

#include "SkaleCommon.h"
#include "thirdparty/lrucache.hpp"



class Schain;

namespace leveldb {
class DB;

class Status;

class Slice;
}  // namespace leveldb



class CacheLevelDB {

    static list<uint64_t> writeTimes;
    static recursive_mutex writeTimeMutex;
    static atomic<uint64_t> writeTimeTotal;
    static list<uint64_t> readTimes;
    static recursive_mutex readTimeMutex;
    static atomic<uint64_t> readTimeTotal;
    static atomic<uint64_t> readCounter;
    static atomic<uint64_t> writeCounter;

protected:

    vector< ptr< leveldb::DB > > db;
    uint64_t highestDBIndex = 0;
    shared_timed_mutex m;

    node_id nodeId = 0;
    string prefix;
    string dirname;
    uint64_t maxDBSize = 0;
    uint64_t totalSigners = 0;
    uint64_t requiredSigners = 0;
    bool isDuplicateAddOK = false;
    Schain* sChain = nullptr;

    void verify();


    ptr< map< schain_index, string > > writeByteArrayToSetUnsafe(
        const char* _value, uint64_t _valueLen, block_id _blockId, schain_index _index );
    string index2Path( uint64_t index );

    string readString( string& _key );
    string readStringUnsafe( string& _key );

    void writeString( const string& key1, const string& value1, bool overWrite = false );

    ptr< map< schain_index, string > > writeStringToSet(
        const string& _value, block_id _blockId, schain_index _index );

    ptr< map< schain_index, string > > writeByteArrayToSet(
        const char* _value, uint64_t _valueLen, block_id _blockId, schain_index _index );

    ptr< map< schain_index, string > > readSet( block_id _blockId );

    ptr< map< schain_index, string > > readSetUnsafe( block_id _blockId );

    void writeByteArray( const char* _key, size_t _keyLen, const char* _value, size_t _valueLen );
    void writeByteArray( string& _key, const ptr< vector< uint8_t > >& _data );

    string createKey( block_id _blockId );

    string createKey( block_id _blockId, schain_index _proposerIndex );

    string createKey( block_id _blockId, uint64_t _counter );

    string createKey( const block_id& _blockId, const schain_index& _proposerIndex,
                      const bin_consensus_round& _round );

    string createCounterKey( block_id _block_id );

    bool keyExists( const string& _key );

    bool keyExistsUnsafe( const string& _key );

    bool keyExistsInSet( block_id _blockId, schain_index _index );

    string readStringFromBlockSet( block_id _blockId, schain_index _index );

    void rotateDBsIfNeeded();

    ptr< leveldb::DB > openDB( uint64_t _index );

    uint64_t readCount( block_id _blockId );

    bool isEnough( block_id _blockID );


    CacheLevelDB( Schain* _sChain, string& _dirName, string& _prefix, node_id _nodeId,
                  uint64_t _maxDBSize, bool _isDuplicateAddOK = false );

    static ptr< map< string, string > > readPrefixRangeFromDBUnsafe(
        string& _prefix, const ptr< leveldb::DB >& _db, bool lastOnly = false );

public:


    void destroy();

    virtual const string& getFormatVersion() = 0;

    static void throwExceptionOnError( leveldb::Status& result );

    pair< uint64_t, uint64_t > findMaxMinDBIndex();

    [[nodiscard]] Schain* getSchain() const;

    class KeyVisitor {
    public:
        virtual void visitDBKey( const char* _data ) = 0;
    };

    uint64_t visitKeys( KeyVisitor* _visitor, uint64_t _maxKeysToVisit );

    virtual ~CacheLevelDB();

    uint64_t getActiveDBSize();

    ptr< map< string, string > > readPrefixRange( string& _prefix );

    static void addWriteStats(uint64_t _time);
    static void addReadStats(uint64_t _time);

    static  uint64_t getReadStats();
    static  uint64_t getWriteStats();

    static uint64_t getReads() {
        return readCounter;
    }

    static uint64_t getWrites() {
        return writeCounter;
    }


    void checkForDeadLock(const char *_functionName);

    void checkForDeadLockRead(const char *_functionName);
};


#endif  // SKALED_CACHELEVELDB_H
