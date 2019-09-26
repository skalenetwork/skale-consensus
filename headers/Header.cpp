/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file Header.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/InvalidArgumentException.h"

#include "../abstracttcpserver/ConnectionStatus.h"

#include "../thirdparty/json.hpp"

#include "../abstracttcpserver/AbstractServerAgent.h"
#include "../exceptions/ParsingException.h"
#include "../exceptions/NetworkProtocolException.h"

#include "../network/Buffer.h"
#include "../network/IO.h"

#include "Header.h"



bool Header::isComplete() const {
    return complete;
}

ptr<Buffer> Header::toBuffer() {
    ASSERT(complete);
    nlohmann::json j;

    CHECK_STATE(type != nullptr);

    j["type"] = type;
    j["status"] = status;

    j["substatus"] = substatus;

    addFields(j);

    string s = j.dump();

    uint64_t len = s.size();

    CHECK_STATE(len > 16);

    auto buf = make_shared<Buffer>(len + sizeof(uint64_t));
    buf->write(&len, sizeof(len));
    buf->write((void *) s.data(), s.size());

    CHECK_STATE(buf->getCounter() > 16);

    return buf;
}



void Header::nullCheck(nlohmann::json &js, const char *name) {
    if (js[name].is_null()) {
        BOOST_THROW_EXCEPTION(NetworkProtocolException("Null " + string(name) + " in json", __CLASS_NAME__));
    }
};

uint64_t Header::getUint64(nlohmann::json &_js, const char *_name) {
    nullCheck(_js, _name);
    uint64_t result = _js[_name];
    return result;
};


uint32_t Header::getUint32(nlohmann::json &_js, const char *_name) {
    nullCheck(_js, _name);
    uint32_t result = _js[_name];
    return result;
};

ptr<string> Header::getString(nlohmann::json &_js, const char *_name) {
    nullCheck(_js, _name);
    string result = _js[_name];
    return make_shared<string>(result);
}

Header::Header(const char *_type) : type(_type){
    totalObjects++;
}



Header::~Header() {
    totalObjects--;
}


atomic<uint64_t>  Header::totalObjects(0);
