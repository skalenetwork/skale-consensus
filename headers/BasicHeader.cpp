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

    @file BasicHeader.cpp
    @author Stan Kladko
    @date 2018
*/



#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"
#include "exceptions/InvalidArgumentException.h"
#include "exceptions/NetworkProtocolException.h"
#include "network/Buffer.h"
#include "BasicHeader.h"

bool BasicHeader::isComplete() const {
    return complete;
}

string BasicHeader::serializeToString() {
    CHECK_STATE(complete)

    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();

    CHECK_STATE(type);

    writer.String("type");
    writer.String(type);


    addFields(writer);


    writer.EndObject();
    writer.Flush();
    string s(sb.GetString());

    CHECK_STATE(s.size() > 16);

    return s;
}

int64_t BasicHeader::getTotalObjects() {
    return totalObjects;
}

ptr< Buffer > BasicHeader::toBuffer() {

    auto s = serializeToString();

    uint64_t len  = s.size();

    auto buf = make_shared<Buffer>(len + sizeof(len));
    buf->write(&len, sizeof(len));
    buf->write((void *) s.data(), len);
    CHECK_STATE(buf->getCounter() >= 10);
    return buf;
}


uint64_t BasicHeader::getUint64Rapid(const rapidjson::Value &_d, const char *_name) {
    CHECK_ARGUMENT(_name);
    CHECK_STATE(_d.HasMember(_name));
    const rapidjson::Value& a = _d[_name];
    CHECK_STATE(a.IsUint64());
    return a.GetUint64();
};

uint32_t BasicHeader::getUint32Rapid(const rapidjson::Value &_d, const char *_name) {
    CHECK_ARGUMENT(_name);
    CHECK_STATE(_d.HasMember(_name));
    const rapidjson::Value& a = _d[_name];
    CHECK_STATE(a.IsUint());
    return a.GetUint();
};

int32_t BasicHeader::getInt32Rapid(const rapidjson::Value &_d, const char *_name) {
    CHECK_ARGUMENT(_name);
    CHECK_STATE(_d.HasMember(_name));
    const rapidjson::Value& a = _d[_name];
    CHECK_STATE(a.IsInt());
    return a.GetInt();
};

vector<uint64_t> BasicHeader::getUint64ArrayRapid( const rapidjson::Value& _d, const char* _name ) {
    vector<uint64_t> result;
    CHECK_ARGUMENT(_name);
    CHECK_STATE(_d.HasMember(_name))
    const rapidjson::Value& a = _d[_name];;
    CHECK_STATE(a.IsArray());

    for (const auto& element : a.GetArray()) {
        CHECK_STATE( element.IsUint64() );
        result.push_back( element.GetUint64() );
    }

    return result;
}


string BasicHeader::getStringRapid(const rapidjson::Value &_d, const char *_name) {
    CHECK_ARGUMENT(_name);
    CHECK_STATE(_d.HasMember(_name));
    CHECK_STATE(_d[_name].IsString());
    return _d[_name].GetString();
};



BasicHeader::BasicHeader(const char *_type) : type(_type)  {
    CHECK_ARGUMENT(_type);
    totalObjects++;
}


BasicHeader::~BasicHeader() {
    if (totalObjects > 0)
        totalObjects--;
}


atomic<int64_t>  BasicHeader::totalObjects(1);