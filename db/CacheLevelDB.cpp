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

    @file LevelDB.cpp
    @file LevelDB.cpp
    @author Stan Kladko
    @date 2019
*/

#include "RandomDB.h"
#include "ProposalHashDB.h"
#include "BlockDB.h"
#include "datastructures/CommittedBlock.h"
#include "exceptions/InvalidStateException.h"


#include "exceptions/ExitRequestedException.h"

#include "SkaleCommon.h"
#include "Log.h"
#include "leveldb/db.h"
#include "leveldb/write_batch.h"
#include "thirdparty/json.hpp"

#include "chains/Schain.h"
#include "datastructures/TransactionList.h"
#include "datastructures/Transaction.h"
#include "datastructures/PartialHashesList.h"
#include "node/Node.h"
#include "datastructures/BlockProposal.h"
#include "crypto/BLAKE3Hash.h"
#include "exceptions/LevelDBException.h"



#include "monitoring/LivelinessMonitor.h"

#include "CacheLevelDB.h"
#include "leveldb/cache.h"




using namespace leveldb;


static WriteOptions writeOptions; // NOLINT(cert-err58-cpp)
static ReadOptions readOptions; // NOLINT(cert-err58-cpp)

string CacheLevelDB::path2Index(uint64_t index){
    return dirname + "/db." + to_string(index);
}

string CacheLevelDB::createKey(const block_id _blockId, uint64_t _counter) {
    return  getFormatVersion() + ":" + to_string(_blockId) + ":" + to_string(_counter);
}

string CacheLevelDB::createKey(const block_id _blockId) {
    return getFormatVersion() + ":" + to_string(_blockId);
}


string CacheLevelDB::createKey(block_id _blockId, schain_index _proposerIndex) {
    return
        getFormatVersion() + ":" + to_string(_blockId) + ":" + to_string(_proposerIndex);
}


string
CacheLevelDB::createKey(const block_id &_blockId, const schain_index &_proposerIndex,
                        const bin_consensus_round &_round) {
    return getFormatVersion() + ":" + to_string(_blockId) + ":" + to_string(_proposerIndex) + ":" +
           to_string(_round);
}


string CacheLevelDB::createCounterKey(block_id _blockId) {
    return getFormatVersion() + ":COUNTER:" + to_string(_blockId);
}


string CacheLevelDB::readStringFromBlockSet(block_id _blockId, schain_index _index) {
    auto key = createKey(_blockId, _index);
    return readString(key);
}


bool CacheLevelDB::keyExistsInSet(block_id _blockId, schain_index _index) {
    auto key = createKey(_blockId, _index);
    CHECK_STATE(!key.empty())
    return keyExists(key);
}

Schain *CacheLevelDB::getSchain() const {
    CHECK_STATE(sChain)
    return sChain;
}

string CacheLevelDB::readString(string &_key) {
    shared_lock<shared_mutex> lock(m);
    return readStringUnsafe(_key);
}

#include "utils/Time.h"

string CacheLevelDB::readStringUnsafe(string &_key) {

    uint64_t time = 0;

    readCounter.fetch_add(1);
    auto measureTime = (readCounter % 100 == 0);
    if (measureTime)
        time = Time::getCurrentTimeMs();

    for (int i = LEVELDB_SHARDS - 1; i >= 0; i--) {
        string result;
        CHECK_STATE(db.at(i))
        auto status = db.at(i)->Get(readOptions, _key, &result);
        throwExceptionOnError(status);
        if (!status.IsNotFound()) {
            if (measureTime)
                CacheLevelDB::addReadStats(Time::getCurrentTimeMs() - time);
            return result;
        }
    }


    return "";
}

bool CacheLevelDB::keyExistsUnsafe(const string &_key) {
    for (int i = LEVELDB_SHARDS - 1; i >= 0; i--) {
        auto result = make_shared<string>();
        CHECK_STATE(db[i])
        auto status = db.at(i)->Get(readOptions, _key, result.get());
        throwExceptionOnError(status);
        if (!status.IsNotFound())
            return true;
    }

    return false;
}

bool CacheLevelDB::keyExists(const string &_key) {

    shared_lock<shared_mutex> lock(m);

    return keyExistsUnsafe(_key);
}


void CacheLevelDB::writeString(const string &_key, const string &_value,
                               bool _overWrite) {

    rotateDBsIfNeeded();

    uint64_t time = 0;
    writeCounter.fetch_add(1);
    auto measureTime = (writeCounter % 100 == 0);
    if (measureTime)
        time = Time::getCurrentTimeMs();


    {
        lock_guard<shared_mutex> lock(m);

        if ((!_overWrite) && keyExistsUnsafe(_key))
        {
            LOG(trace, "Double db entry " + this->prefix + "\n" + _key);
            return;
        }

        auto status = db.back()->Put(writeOptions, _key, Slice(_value));

        throwExceptionOnError(status);
    }

    if (measureTime)
        CacheLevelDB::addWriteStats(Time::getCurrentTimeMs() - time);
}


void CacheLevelDB::writeByteArray(const char *_key, size_t _keyLen, const char * _value,
                                  size_t _valueLen) {

    CHECK_ARGUMENT(_key)
    CHECK_ARGUMENT(_value)

    rotateDBsIfNeeded();

    uint64_t time = 0;
    writeCounter.fetch_add(1);
    auto measureTime = (writeCounter % 100 == 0);
    if (measureTime)
        time = Time::getCurrentTimeMs();


    {
        lock_guard<shared_mutex> lock(m);

        if (keyExistsUnsafe(string(_key))) {
            LOG(trace, "Double entry written to db");
            return;
        }

        auto status = db.back()->Put(writeOptions, Slice(_key, _keyLen), Slice( _value, _valueLen));

        throwExceptionOnError(status);
    }

    if (measureTime)
        CacheLevelDB::addWriteStats(Time::getCurrentTimeMs() - time);
}

void CacheLevelDB::writeByteArray(string &_key, const ptr<vector<uint8_t>>& _data) {

    CHECK_ARGUMENT(_data)

    rotateDBsIfNeeded();

    writeCounter.fetch_add(1);
    uint64_t time = 0;
    auto measureTime = (writeCounter % 100 == 0);
    if (measureTime)
        time = Time::getCurrentTimeMs();

    auto value = (const char *) _data->data();
    auto valueLen = _data->size();

    {
        lock_guard<shared_mutex> lock(m);
        auto status = db.back()->Put(writeOptions, Slice(_key), Slice(value, valueLen));
        throwExceptionOnError(status);
    }


    if (measureTime)
        CacheLevelDB::addWriteStats(Time::getCurrentTimeMs() - time);
}

void CacheLevelDB::throwExceptionOnError(Status& _status) {
    if (_status.IsNotFound())
        return;

    if (!_status.ok()) {
        BOOST_THROW_EXCEPTION(LevelDBException("Could not write to database:" +
                                               _status.ToString(), __CLASS_NAME__));
    }

}


ptr<map<string, string>> CacheLevelDB::readPrefixRange(string &_prefix) {

    ptr<map<string, string>> result = nullptr;

    shared_lock<shared_mutex> lock(m);

    for (int i = LEVELDB_SHARDS - 1; i >= 0; i--) {
        CHECK_STATE(db.at(i))
        auto partialResult = readPrefixRangeFromDBUnsafe(_prefix, db[i]);
        if (partialResult) {
            if (result) {
                result->insert(partialResult->begin(), partialResult->end());
            } else {
                result = partialResult;
            }
        }
    }


    return result;


}

ptr<map<string, string>> CacheLevelDB::readPrefixRangeFromDBUnsafe(string &_prefix, const ptr<leveldb::DB>& _db,
                                                                   bool _lastOnly) {

    CHECK_ARGUMENT(_db)

    ptr<map<string, string>> result = make_shared<map<string, string>>();

    auto idb = ptr<Iterator>(_db->NewIterator(readOptions));

    if (_lastOnly) {
        idb->SeekToLast();
        if (idb->Valid()) {
            (*result)[idb->key().ToString()] = idb->value().ToString();
        }
        return result;
    }

    for (idb->Seek(_prefix); idb->Valid() && idb->key().starts_with(_prefix); idb->Next()) {
        (*result)[idb->key().ToString()] = idb->value().ToString();
    }


    return result;
}


uint64_t CacheLevelDB::visitKeys(CacheLevelDB::KeyVisitor *_visitor, uint64_t _maxKeysToVisit) {

    CHECK_ARGUMENT(_visitor)

    shared_lock<shared_mutex> lock(m);

    uint64_t readC = 0;

    auto it = unique_ptr<leveldb::Iterator>(db.back()->NewIterator(readOptions));
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        _visitor->visitDBKey(it->key().data());
        readCounter++;
        if (readCounter >= _maxKeysToVisit) {
            break;
        }
    }

    return readC;
}


shared_ptr<leveldb::DB> CacheLevelDB::openDB(uint64_t _index) {

    try {
        leveldb::DB *dbase = nullptr;

        static leveldb::Options options;

        auto maxFiles = getSchain()->getNode()->getParamInt64(
            "maxOpenLeveldbFiles", 0);

        if (maxFiles != 0) {
            options.max_open_files = maxFiles;
        }


        auto blockCache = getSchain()->getNode()->getParamInt64(
            "levelDbBlockCache", 0);

        if (blockCache != 0) {
            options.block_cache = leveldb::NewLRUCache(blockCache * 1048576);
        }

        options.create_if_missing = true;

        CHECK_STATE2(leveldb::DB::Open(options, path2Index( _index ),
                                       &dbase).ok(),
                     "Unable to open database")
        CHECK_STATE(dbase)

        return ptr<DB>(dbase);

    } catch (ExitRequestedException &e) { throw; }
    catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


CacheLevelDB::CacheLevelDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize,
                           bool _isDuplicateAddOK) {



    CHECK_ARGUMENT(_sChain);

    this->sChain = _sChain;
    this->nodeId = _nodeId;
    this->prefix = _prefix;
    this->totalSigners = _sChain->getTotalSigners();
    this->requiredSigners = _sChain->getRequiredSigners();
    this->dirname = _dirName + "/" + _prefix;
    this->maxDBSize = _maxDBSize;
    this->isDuplicateAddOK = _isDuplicateAddOK;

    boost::filesystem::path path(dirname);
    boost::filesystem::create_directory(path);

    CHECK_ARGUMENT(_maxDBSize != 0);

    highestDBIndex = findMaxMinDBIndex().first;

    if (highestDBIndex < LEVELDB_SHARDS ) {
        highestDBIndex = LEVELDB_SHARDS;
    }


    for (auto i = highestDBIndex - LEVELDB_SHARDS + 1; i <= highestDBIndex; i++) {
        auto dbase = openDB(i);
        CHECK_STATE(dbase);
        db.push_back(dbase);
    }

    verify();
}

CacheLevelDB::~CacheLevelDB() {
}

using namespace boost::filesystem;

uint64_t CacheLevelDB::getActiveDBSize() {

    try {
        vector<path> files;

        path levelDBPath( path2Index( highestDBIndex ));

        if (!is_directory(levelDBPath)) {
            return 0;
        }


        copy(directory_iterator(levelDBPath), directory_iterator(), back_inserter(files));

        uint64_t size = 0;

        for (auto &filePath : files) {
            if (is_regular_file(filePath)) {
                size = size + file_size(filePath);
            }
        }
        return size;

    } catch (ExitRequestedException &e) { throw; }
    catch (exception &) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


pair<uint64_t, uint64_t> CacheLevelDB::findMaxMinDBIndex() {

    vector<path> dirs;
    vector<uint64_t> indices;

    copy(directory_iterator(path(dirname)), directory_iterator(), back_inserter(dirs));
    sort(dirs.begin(), dirs.end());

    size_t offset = string("db.").size();

    for (auto &path : dirs) {
        if (is_directory(path)) {
            auto fileName = path.filename().string();
            if (fileName.find("db.") == 0) {
                auto index = fileName.substr(offset);
                auto value = strtoull(index.c_str(), nullptr, 10);
                if (value != 0) {
                    indices.push_back(value);
                }
            }
        }
    }

    if (indices.size() == 0)
        return {0, 0};

    auto maxIndex = *max_element(begin(indices), end(indices));
    auto minIndex = *min_element(begin(indices), end(indices));

    return {maxIndex, minIndex};
}

void CacheLevelDB::rotateDBsIfNeeded() {

    try {

        if (getActiveDBSize() <= maxDBSize)
            return;
        {
            lock_guard<shared_mutex> lock(m);

            if (getActiveDBSize() <= maxDBSize)
                return;

            LOG(info, "Rotating " + prefix + " database. Max DB size in bytes: "
                      + to_string(maxDBSize));

            auto newDB = openDB(highestDBIndex + 1);

            for (uint64_t i = 1; i < LEVELDB_SHARDS; i++) {
                db.at(i - 1) = nullptr;
                db.at(i - 1) = db.at(i);
            }

            db[LEVELDB_SHARDS - 1] = shared_ptr<leveldb::DB>(newDB);

            highestDBIndex++;

            uint64_t minIndex;

            while ((minIndex = findMaxMinDBIndex().second) + LEVELDB_SHARDS <= highestDBIndex) {

                if (minIndex == 0) {
                    return;
                }

                auto dbName = path2Index( minIndex );
                try {

                    boost::filesystem::remove_all(path(dbName));
                } catch (SkaleException &e) {
                    LOG(err, "Could not remove db:" + dbName);
                }
            }

            verify();
        }

    } catch (ExitRequestedException &e) { throw; }
    catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


bool CacheLevelDB::isEnough(block_id _blockID) {
    return readCount(_blockID) >= requiredSigners;
}


uint64_t CacheLevelDB::readCount(block_id _blockId) {

    auto counterKey = createCounterKey(_blockId);

    auto countString = readString(counterKey);

    if (countString == "") {
        return 0;
    }

    try {

        auto result = stoull(countString, NULL, 10);

        CHECK_STATE(result <= totalSigners);

        return result;

    } catch (...) {
        LOG(err, "Incorrect value in LevelDB:" + countString);
        return 0;
    }
}

ptr<map<schain_index, string>>
CacheLevelDB::writeStringToSet(const string &_value, block_id _blockId, schain_index _index) {
    return writeByteArrayToSet(_value.data(), _value.size(), _blockId,
                               _index);
}

ptr<map<schain_index, string>>
CacheLevelDB::readSet(block_id _blockId) {


    shared_lock<shared_mutex> lock(m);

    return readSetUnsafe(_blockId);

}


ptr<map<schain_index, string>>
CacheLevelDB::writeByteArrayToSet(const char *_value, uint64_t _valueLen, block_id _blockId, schain_index _index) {


    rotateDBsIfNeeded();


    {

        lock_guard<shared_mutex> lock(m);

        return writeByteArrayToSetUnsafe(_value, _valueLen, _blockId, _index);

    }

}


ptr<map<schain_index, string>>
CacheLevelDB::readSetUnsafe(block_id _blockId) {

    auto enoughSet = make_shared<map<schain_index, string>>();

    for (uint64_t i = 1; i <= totalSigners; i++) {
        auto key = createKey(_blockId, schain_index(i));
        auto entry = readStringUnsafe(key);

        if (entry != "")
            (*enoughSet)[schain_index(i)] = entry;
        if (enoughSet->size() == requiredSigners) {
            break;
        }
    }

    return enoughSet;

}


ptr<map<schain_index, string>>
CacheLevelDB::writeByteArrayToSetUnsafe(const char *_value, uint64_t _valueLen, block_id _blockId,
                                        schain_index _index) {



    CHECK_ARGUMENT(_index > 0 && _index <= totalSigners);

    uint64_t time = 0;
    writeCounter.fetch_add(1);
    auto measureTime = (writeCounter % 100 == 0);
    if (measureTime)
        time = Time::getCurrentTimeMs();

    auto entryKey = createKey(_blockId, _index);
    CHECK_STATE(entryKey != "");


    if (keyExistsUnsafe(entryKey)) {
        if (!isDuplicateAddOK)
            LOG(trace, "Double db entry " + this->prefix + "\n" + to_string(_blockId) + ":" + to_string(_index));
        return nullptr;
    }

    uint64_t count = 0;

    ptr<leveldb::DB> containingDb = nullptr;
    auto result = make_shared<string>();

    auto counterKey = createCounterKey(_blockId);

    for (int i = LEVELDB_SHARDS - 1; i >= 0; i--) {
        CHECK_STATE(db[i]);
        auto status = db[i]->Get(readOptions, counterKey, &*result);
        throwExceptionOnError(status);
        if (!status.IsNotFound()) {
            containingDb = db.at(i);
            break;
        }
    }

    if (containingDb != nullptr) {
        try {
            count = stoull(*result, NULL, 10);
        } catch (...) {
            LOG(err, "Incorrect value in LevelDB:" + *result);
            return 0;
        }
    } else {
        containingDb = db.back();
    }
    {

        leveldb::WriteBatch batch;
        count++;

        batch.Put(counterKey, to_string(count));
        batch.Put(entryKey, Slice(_value, _valueLen));
        CHECK_STATE2(containingDb->Write(writeOptions, &batch).ok(), "Could not write LevelDB");
    }


    if (count != requiredSigners) {
        return nullptr;
    }

    auto enoughSet = make_shared<map<schain_index, string>>();

    for (uint64_t i = 1; i <= totalSigners; i++) {
        auto key = createKey(_blockId, schain_index(i));
        auto entry = readStringUnsafe(key);

        if (entry != "")
            (*enoughSet)[schain_index(i)] = entry;
        if (enoughSet->size() == requiredSigners) {
            break;
        }
    }

    CHECK_STATE(enoughSet->size() == requiredSigners);

    if (measureTime)
        CacheLevelDB::addWriteStats(Time::getCurrentTimeMs() - time);

    return enoughSet;

}

void CacheLevelDB::verify() {

    CHECK_STATE(db.size() == LEVELDB_SHARDS );
    for (auto &&x : db) {
        CHECK_STATE(x);
    }
}

void CacheLevelDB::addWriteStats(uint64_t _time) {

    CacheLevelDB::writeTimeTotal.fetch_add(_time);
    LOCK(writeTimeMutex);
    CacheLevelDB::writeTimes.push_back(_time);
    if (writeTimes.size() > LEVELDB_STATS_HISTORY) {
        writeTimeTotal.fetch_sub(writeTimes.front());
        writeTimes.pop_front();
    }
}

void CacheLevelDB::addReadStats(uint64_t _time) {

    CacheLevelDB::readTimeTotal.fetch_add(_time);
    LOCK(readTimeMutex);
    CacheLevelDB::readTimes.push_back(_time);
    if (readTimes.size() > LEVELDB_STATS_HISTORY) {
        readTimeTotal.fetch_sub(readTimes.front());
        readTimes.pop_front();
    }
}



list<uint64_t> CacheLevelDB::writeTimes;
recursive_mutex CacheLevelDB::writeTimeMutex;
atomic<uint64_t> CacheLevelDB::writeTimeTotal = 0;
list<uint64_t> CacheLevelDB::readTimes;
recursive_mutex CacheLevelDB::readTimeMutex;
atomic<uint64_t> CacheLevelDB::readTimeTotal = 0;

atomic<uint64_t> CacheLevelDB::readCounter = 0;
atomic<uint64_t> CacheLevelDB::writeCounter = 0;

uint64_t CacheLevelDB::getReadStats() {
    return readTimeTotal;
}

uint64_t CacheLevelDB::getWriteStats() {
    return readTimeTotal;
}