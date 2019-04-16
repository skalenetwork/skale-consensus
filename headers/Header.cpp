//
// Created by stan on 11.08.18.
//
#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
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

    j["status"] = status;

    j["substatus"] = substatus;

    addFields(j);

    string s = j.dump();

    uint64_t len = s.size();

    ASSERT(len > 16);

    auto buf = make_shared<Buffer>(len + sizeof(uint64_t));
    buf->write(&len, sizeof(len));
    buf->write((void *) s.data(), s.size());

    ASSERT(buf->getCounter() > 16);

    return buf;
}



void Header::nullCheck(nlohmann::json &js, const char *name) {
    if (js[name].is_null()) {
        BOOST_THROW_EXCEPTION(NetworkProtocolException("Null " + string(name) + " in the request", __CLASS_NAME__));
    }
};

uint64_t Header::getUint64(nlohmann::json &_js, const char *_name) {
    nullCheck(_js, _name);
    uint64_t result = _js[_name];
    return result;
};

ptr<string> Header::getString(nlohmann::json &_js, const char *_name) {
    nullCheck(_js, _name);
    string result = _js[_name];
    return make_shared<string>(result);
}

Header::Header() {

    totalObjects++;
    status = CONNECTION_SERVER_ERROR;
    substatus = CONNECTION_ERROR_UNKNOWN_SERVER_ERROR;


}



Header::~Header() {
    totalObjects--;
}


atomic<uint64_t>  Header::totalObjects(0);
