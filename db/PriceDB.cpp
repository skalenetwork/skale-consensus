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

    @file PriceDB.cpp
    @author Stan Kladko
    @date 2019
*/


#include "../SkaleCommon.h"
#include "../Log.h"


#include "PriceDB.h"


PriceDB::PriceDB(
        string& _dirName,
        string &_prefix, node_id _nodeId) : LevelDB(_dirName, _prefix, _nodeId) {}


const string PriceDB::getFormatVersion() {
    return "1.0";
}


ptr<string> PriceDB::createKey(block_id _blockId) {
    return make_shared<string>(getFormatVersion() + ":" + to_string(_blockId));
}

u256 PriceDB::readPrice(block_id _blockID) {

    auto key = createKey(_blockID);

    auto price = readString(*key);

    if (price == nullptr) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Price for this block is unknown", __CLASS_NAME__));
    }

    return u256(price->c_str());
}


void PriceDB::savePrice(u256 _price, block_id _blockID) {

    auto key = createKey(_blockID);

    auto value = _price.str();

    writeString(*key, value);
}

