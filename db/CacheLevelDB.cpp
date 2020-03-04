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
#include "thirdparty/lrucache.hpp"

#include "exceptions/ExitRequestedException.h"

#include "SkaleCommon.h"
#include "Log.h"
#include "thirdparty/json.hpp"
#include "leveldb/db.h"
#include "leveldb/write_batch.h"

#include "chains/Schain.h"
#include "datastructures/TransactionList.h"
#include "datastructures/Transaction.h"
#include "datastructures/PartialHashesList.h"
#include "node/Node.h"
#include "datastructures/BlockProposal.h"
#include "crypto/SHAHash.h"
#include "exceptions/LevelDBException.h"
#include "exceptions/FatalError.h"


#include "monitoring/LivelinessMonitor.h"

#include "CacheLevelDB.h"


using namespace leveldb;


static WriteOptions writeOptions;
static ReadOptions readOptions;

std::string CacheLevelDB::path_to_index(uint64_t index){
    return dirname + "/db." + to_string(index);
}

ptr<string> CacheLevelDB::createKey(const block_id _blockId, uint64_t _counter) {
    return make_shared<string>(
            getFormatVersion() + ":" + to_string(_blockId) + ":" + to_string(_counter));
}

ptr<string> CacheLevelDB::createKey(const block_id _blockId) {
    return make_shared<string>(getFormatVersion() + ":" + to_string(_blockId));
}


ptr<string> CacheLevelDB::createKey(block_id _blockId, schain_index _proposerIndex) {
    return make_shared<string>(
            getFormatVersion() + ":" + to_string(_blockId) + ":" + to_string(_proposerIndex));
}

ptr<string>
CacheLevelDB::createKey(const block_id &_blockId, const schain_index &_proposerIndex,
                        const bin_consensus_round &_round) {
    return make_shared<string>(getFormatVersion() + ":" + to_string(_blockId) + ":" + to_string(_proposerIndex) + ":" +
                               to_string(_round));
}


string CacheLevelDB::createSetKey(block_id _blockId, schain_index _index) {
    return getFormatVersion() + ":" + to_string(_blockId) + ":" + to_string(_index);
}

string CacheLevelDB::createCounterKey(block_id _blockId) {
    return getFormatVersion() + ":COUNTER:" + to_string(_blockId);
}


ptr<string> CacheLevelDB::readStringFromBlockSet(block_id _blockId, schain_index _index) {
    auto key = createSetKey(_blockId, _index);
    return readString(key);
}


bool CacheLevelDB::keyExistsInSet(block_id _blockId, schain_index _index) {
    return keyExists(createSetKey(_blockId, _index));
}

Schain *CacheLevelDB::getSchain() const {
    return sChain;
}

ptr<string> CacheLevelDB::readString(string &_key) {
    shared_lock<shared_mutex> lock(m);
    return readStringUnsafe(_key);
}


ptr<string> CacheLevelDB::readStringUnsafe(string &_key) {

    for (int i = LEVELDB_PIECES - 1; i >= 0; i--) {
        auto result = make_shared<string>();
        ASSERT(db[i] != nullptr);
        auto status = db[i]->Get(readOptions, _key, &*result);
        throwExceptionOnError(status);
        if (!status.IsNotFound())
            return result;
    }

    return nullptr;
}

bool CacheLevelDB::keyExistsUnsafe(const string &_key) {

    for (int i = LEVELDB_PIECES - 1; i >= 0; i--) {
        auto result = make_shared<string>();
        ASSERT(db[i] != nullptr);
        auto status = db[i]->Get(readOptions, _key, &*result);
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

    {
        shared_lock<shared_mutex> lock(m);

        if ((!_overWrite) && keyExistsUnsafe(_key)) {
            LOG(trace, "Double db entry " + this->prefix + "\n" + _key);
            return;
        }

        auto status = db.back()->Put(writeOptions, _key, Slice(_value));

        throwExceptionOnError(status);
    }
}


void CacheLevelDB::writeByteArray(const char *_key, size_t _keyLen, const char *value,
                                  size_t _valueLen) {

    rotateDBsIfNeeded();

    {
        shared_lock<shared_mutex> lock(m);

        if (keyExistsUnsafe(string(_key))) {
            LOG(trace, "Double entry written to db");
            return;
        }

        auto status = db.back()->Put(writeOptions, Slice(_key, _keyLen), Slice(value, _valueLen));

        throwExceptionOnError(status);
    }
}

void CacheLevelDB::writeByteArray(string &_key, ptr<vector<uint8_t>> _data) {

    CHECK_ARGUMENT(_data);


    rotateDBsIfNeeded();

    auto value = (const char *) _data->data();
    auto valueLen = _data->size();

    {
        shared_lock<shared_mutex> lock(m);
        auto status = db.back()->Put(writeOptions, Slice(_key), Slice(value, valueLen));
        throwExceptionOnError(status);
    }
}

void CacheLevelDB::throwExceptionOnError(Status _status) {
    if (_status.IsNotFound())
        return;

    if (!_status.ok()) {
        BOOST_THROW_EXCEPTION(LevelDBException("Could not write to database:" +
                                               _status.ToString(), __CLASS_NAME__));
    }

}

ptr<string> CacheLevelDB::readLastKeyInPrefixRange(string &_prefix) {

    ptr<map<string, ptr<string>>> result = nullptr;

    shared_lock<shared_mutex> lock(m);

    for (int i = LEVELDB_PIECES - 1; i >= 0; i--) {
        ASSERT(db[i]);
        auto partialResult = readPrefixRangeFromDBUnsafe(_prefix, db[i], true);
        if (partialResult) {
            if (result) {
                result->insert(partialResult->begin(), partialResult->end());
            } else {
                result = partialResult;
            }
        }
    }

    if (result->empty()) {
        return nullptr;
    }

    return make_shared<string>(result->rbegin()->first);

}


ptr<map<string, ptr<string>>> CacheLevelDB::readPrefixRange(string &_prefix) {


    ptr<map<string, ptr<string>>> result = nullptr;

    shared_lock<shared_mutex> lock(m);

    for (int i = LEVELDB_PIECES - 1; i >= 0; i--) {
        ASSERT(db[i]);
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

ptr<map<string, ptr<string>>> CacheLevelDB::readPrefixRangeFromDBUnsafe(string &_prefix, ptr<leveldb::DB> _db,
                                                                        bool _lastOnly) {

    CHECK_ARGUMENT(_db);

    ptr<map<string, ptr<string>>> result = make_shared<map<string, ptr<string>>>();

    auto idb = ptr<Iterator>(_db->NewIterator(readOptions));

    if (_lastOnly) {
        idb->SeekToLast();
        if (idb->Valid()) {
            (*result)[idb->key().ToString()] = make_shared<string>(idb->value().ToString());
            cerr << idb->key().ToString() << endl;
        }
        return result;
    }

    for (idb->Seek(_prefix); idb->Valid() && idb->key().starts_with(_prefix); idb->Next()) {
        (*result)[idb->key().ToString()] = make_shared<string>(idb->value().ToString());
    }


    return result;
}


uint64_t CacheLevelDB::visitKeys(CacheLevelDB::KeyVisitor *_visitor, uint64_t _maxKeysToVisit) {

    shared_lock<shared_mutex> lock(m);

    uint64_t readCounter = 0;

    leveldb::Iterator *it = db.back()->NewIterator(readOptions);
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        _visitor->visitDBKey(it->key().data());
        readCounter++;
        if (readCounter >= _maxKeysToVisit) {
            break;
        }
    }

    delete it;

    return readCounter;
}


DB *CacheLevelDB::openDB(uint64_t _index) {

    try {
        leveldb::DB *dbase = nullptr;

        static leveldb::Options options;
        options.create_if_missing = true;

        ASSERT2(leveldb::DB::Open(options, dirname + "/" + "db." + to_string(_index),
                                  &dbase).ok(),
                "Unable to open database");
        return dbase;

    } catch (ExitRequestedException &e) { throw; }
    catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


CacheLevelDB::CacheLevelDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize,
                           bool _isDuplicateAddOK)
        : nodeId(_nodeId),
          prefix(_prefix),
          totalSigners(_sChain->getTotalSigners()),
          requiredSigners(_sChain->getRequiredSigners()),
          dirname(_dirName + "/" + _prefix),
          maxDBSize(_maxDBSize),
          isDuplicateAddOK(_isDuplicateAddOK), sChain(_sChain) {



    CHECK_STATE(_sChain != nullptr);

    boost::filesystem::path path(dirname);
    boost::filesystem::create_directory(path);

    CHECK_ARGUMENT(_maxDBSize != 0);

    highestDBIndex = findMaxMinDBIndex().first;

    if (highestDBIndex < LEVELDB_PIECES) {
        highestDBIndex = LEVELDB_PIECES;
    }


    for (auto i = highestDBIndex - LEVELDB_PIECES + 1; i <= highestDBIndex; i++) {
        leveldb::DB *dbase = openDB(i);
        db.push_back(shared_ptr<leveldb::DB>(dbase));
    }

    verify();
}

CacheLevelDB::~CacheLevelDB() {
}

using namespace boost::filesystem;

uint64_t CacheLevelDB::getActiveDBSize() {

    try {
        vector<path> files;

        path levelDBPath(path_to_index(highestDBIndex));

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


std::pair<uint64_t, uint64_t> CacheLevelDB::findMaxMinDBIndex() {

    vector<path> dirs;
    vector<uint64_t> indices;

    copy(directory_iterator(path(dirname)), directory_iterator(), back_inserter(dirs));
    sort(dirs.begin(), dirs.end());

    size_t offset = strlen("db.");

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

            LOG(info, "Rotating db");

            auto newDB = openDB(highestDBIndex + 1);

            for (int i = 1; i < LEVELDB_PIECES; i++) {
                db.at(i - 1) = nullptr;
                db.at(i - 1) = db.at(i);
            }

            db[LEVELDB_PIECES - 1] = shared_ptr<leveldb::DB>(newDB);

            highestDBIndex++;

            uint64_t minIndex;

            while ((minIndex = findMaxMinDBIndex().second) + LEVELDB_PIECES <= highestDBIndex) {

                if (minIndex == 0) {
                    return;
                }

                auto dbName = path_to_index(minIndex);
                try {

                    boost::filesystem::remove_all(path(dbName));
                } catch (Exception &e) {
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

    if (countString == nullptr) {
        return 0;
    }

    try {
        auto result = stoull(*countString, NULL, 10);

        CHECK_STATE(result <= totalSigners);

        return result;

    } catch (...) {
        LOG(err, "Incorrect value in LevelDB:" + *countString);
        return 0;
    }
}

ptr<map<schain_index, ptr<string>>>
CacheLevelDB::writeStringToSet(const string &_value, block_id _blockId, schain_index _index) {
    return writeByteArrayToSet(_value.data(), _value.size(), _blockId,
                               _index);
}


ptr<map<schain_index, ptr<string>>>
CacheLevelDB::writeByteArrayToSet(const char *_value, uint64_t _valueLen, block_id _blockId, schain_index _index) {

    rotateDBsIfNeeded();
    {

        shared_lock<shared_mutex> lock(m);

        return writeByteArrayToSetUnsafe(_value, _valueLen, _blockId, _index);

    }

}


ptr<map<schain_index, ptr<string>>>
CacheLevelDB::writeByteArrayToSetUnsafe(const char *_value, uint64_t _valueLen, block_id _blockId,
                                        schain_index _index) {



    assert(_index > 0 && _index <= totalSigners);


    string entryKey = createSetKey(_blockId, _index);


    if (keyExistsUnsafe(entryKey)) {
        if (!isDuplicateAddOK)
            LOG(trace, "Double db entry " + this->prefix + "\n" + to_string(_blockId) + ":" + to_string(_index));
        return nullptr;
    }

    uint64_t count = 0;

    ptr<leveldb::DB> containingDb = nullptr;
    auto result = make_shared<string>();

    auto counterKey = createCounterKey(_blockId);

    for (int i = LEVELDB_PIECES - 1; i >= 0; i--) {
        ASSERT(db[i] != nullptr);
        auto status = db[i]->Get(readOptions, counterKey, &*result);
        throwExceptionOnError(status);
        if (!status.IsNotFound()) {
            containingDb = db[i];
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


    auto enoughSet = make_shared<map<schain_index, ptr<string>>>();

    for (uint64_t i = 1; i <= totalSigners; i++) {
        auto key = createSetKey(_blockId, schain_index(i));
        auto entry = readStringUnsafe(key);

        if (entry != nullptr)
            (*enoughSet)[schain_index(i)] = entry;
        if (enoughSet->size() == requiredSigners) {
            break;
        }
    }

    assert(enoughSet->size() == requiredSigners);

    return enoughSet;

}

void CacheLevelDB::verify() {

    CHECK_STATE(db.size() == LEVELDB_PIECES);
    for (auto &&x : db) {
        CHECK_STATE(x != nullptr);
    }
}


