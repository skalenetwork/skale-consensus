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
    @author Stan Kladko
    @date 2019
*/


#include "../SkaleCommon.h"
#include "../Log.h"
#include "../thirdparty/json.hpp"
#include "leveldb/db.h"

#include "../chains/Schain.h"
#include "../datastructures/TransactionList.h"
#include "../datastructures/Transaction.h"
#include "../datastructures/PartialHashesList.h"
#include "../node/Node.h"
#include "../datastructures/MyBlockProposal.h"
#include "../datastructures/BlockProposal.h"
#include "../crypto/SHAHash.h"
#include "../exceptions/LevelDBException.h"
#include "../exceptions/FatalError.h"

#include "LevelDB.h"


using namespace leveldb;


static WriteOptions writeOptions;
static ReadOptions readOptions;


ptr<string> LevelDB::readString(string &_key) {


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

void LevelDB::writeString(const string &_key, const string &_value) {

    getFrontDBSize();

    auto status = db.front()->Put(writeOptions, Slice(_key), Slice(_value));

    throwExceptionOnError(status);
}

uint64_t LevelDB::getFrontDBSize() {
    static leveldb::Range all("", "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    uint64_t size;
    db.front()->GetApproximateSizes(&all, 1, &size);

    cerr << size << " " << prefix << endl;

    return size;
}

void LevelDB::writeByteArray(const char *_key, size_t _keyLen, const char *value,
                             size_t _valueLen) {

    getFrontDBSize();

    auto status = db.front()->Put(writeOptions, Slice(_key, _keyLen), Slice(value, _valueLen));

    throwExceptionOnError(status);
}

void LevelDB::writeByteArray(string &_key, const char *value,
                             size_t _valueLen) {

    getFrontDBSize();

    auto status = db.front()->Put(writeOptions, Slice(_key), Slice(value, _valueLen));

    throwExceptionOnError(status);
}

void LevelDB::throwExceptionOnError(Status _status) {
    if (_status.IsNotFound())
        return;

    if (!_status.ok()) {
        BOOST_THROW_EXCEPTION(LevelDBException("Could not write to database:" +
                                               _status.ToString(), __CLASS_NAME__));
    }

}

uint64_t LevelDB::visitKeys(LevelDB::KeyVisitor *_visitor, uint64_t _maxKeysToVisit) {

    uint64_t readCounter = 0;

    leveldb::Iterator *it = db.front()->NewIterator(readOptions);
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

LevelDB::LevelDB(string &_dirName, string &_prefix, node_id _nodeId) : nodeId(_nodeId),
                                                                       prefix(_prefix), dirname(_dirName) {

    boost::filesystem::path path(_dirName);

    auto highestDBIndex = findHighestDBIndex(make_shared<string>(
            _prefix + "."), path);

    if (highestDBIndex < LEVELDB_PIECES) {
        highestDBIndex = LEVELDB_PIECES;
    }

    leveldb::Options options;
    options.create_if_missing = true;

    for (int i = highestDBIndex - LEVELDB_PIECES + 1; i <= highestDBIndex; i++) {
        leveldb::DB *dbase = nullptr;
        ASSERT2(leveldb::DB::Open(options, _dirName + "/" + _prefix + "." + to_string(i),
                                  &dbase).ok(),
                "Unable to open blocks database");
        db.push_back(shared_ptr<leveldb::DB>(dbase));
    }
}

LevelDB::~LevelDB() {
}

using namespace boost::filesystem;

int LevelDB::findHighestDBIndex(ptr<string> _prefix, boost::filesystem::path _path) {

    CHECK_ARGUMENT(_prefix != nullptr);

    vector<path> dirs;
    vector<uint64_t> indices;

    copy(directory_iterator(_path), directory_iterator(), back_inserter(dirs));
    sort(dirs.begin(), dirs.end());

    for (auto &path : dirs) {
        if (is_directory(path)) {
            auto fileName = path.filename().string();
            if (fileName.find(*_prefix) == 0) {
                auto index = fileName.substr(_prefix->size());
                auto value = strtoull(index.c_str(), nullptr, 10);
                if (value != 0) {
                    indices.push_back(value);
                }
            }
        }
    }

    if (indices.size() == 0)
        return -1;

    auto result = *max_element(begin(indices), end(indices));

    return result;
}

