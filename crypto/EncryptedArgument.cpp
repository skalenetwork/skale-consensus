//
// Created by kladko on 31.01.22.
//

#include "thirdparty/rapidjson/document.h"
#include "thirdparty/json.hpp"
#include "thirdparty/rapidjson/prettywriter.h" // for stringify JSON

#include "SkaleCommon.h"
#include "Log.h"

#include "headers/BasicHeader.h"
#include "AesCbcKey.h"

#include "EncryptedArgument.h"

EncryptedArgument::EncryptedArgument(ptr<vector<uint8_t>> _rawArgument) {
    CHECK_STATE(_rawArgument);
    CHECK_STATE(_rawArgument->size() > 10);
    CHECK_STATE(_rawArgument->front() == '{');
    uint64_t jsonEnd;
    for (jsonEnd = 0; jsonEnd < _rawArgument->size(); jsonEnd++) {
        if (_rawArgument->at(jsonEnd) == '}')
            break;
    }

    CHECK_STATE(jsonEnd < _rawArgument->size() - 1)

    using namespace rapidjson;

    Document d;

    d.Parse((const char*) _rawArgument->data(), jsonEnd + 1);

    this->timeStamp = BasicHeader::getUint64Rapid(d, "ts");
    this->encryptedAESKey = BasicHeader::getStringRapid(d, "ek");
}
