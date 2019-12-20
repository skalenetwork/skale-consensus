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


#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/ExitRequestedException.h"
#include "PriceDB.h"

#include "chains/Schain.h"

PriceDB::PriceDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize)
        : CacheLevelDB(_sChain, _dirName, _prefix,
                       _nodeId,
                       _maxDBSize, false) {}


const string PriceDB::getFormatVersion() {
    return "1.0";
}



u256 PriceDB::readPrice(block_id _blockID) {

    LOG(trace, "Read price for block" + to_string(_blockID));

    if (_blockID <= 1) {
        return getSchain()->getNode()->getParamUint64(string("DYNAMIC_PRICING_START_PRICE"), 1000);
    }

    try {


        auto key = createKey(_blockID);

        auto price = readString(*key);

        if (price == nullptr) {
            BOOST_THROW_EXCEPTION(InvalidArgumentException("Price for block " +
            to_string(_blockID) + " is unknown", __CLASS_NAME__));
        }

        return u256(price->c_str());

    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void PriceDB::savePrice(u256 _price, block_id _blockID) {

    LOG(trace, "Save price for block" + to_string(_blockID));

    try {

        auto key = createKey(_blockID);

        auto value = _price.str();

        writeString(*key, value);
    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

