//
// Created by kladko on 31.01.22.
//

#include "thirdparty/rapidjson/document.h"
#include "thirdparty/json.hpp"
#include "thirdparty/rapidjson/prettywriter.h" // for stringify JSON

#include "SkaleCommon.h"
#include "Log.h"

#include "headers/BasicHeader.h"
#include "AesCbcKeyIVPair.h"

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

    CHECK_STATE(jsonEnd < _rawArgument->size() - 5)

    using namespace rapidjson;

    Document d;

    d.Parse((const char*) _rawArgument->data(), jsonEnd + 1);

    this->timeStamp = BasicHeader::getUint64Rapid(d, "ts");
    this->encryptedAESKey = BasicHeader::getStringRapid(d, "ek");
    this->iv = BasicHeader::getStringRapid(d, "iv");

    this->aesEncryptedSegment = make_shared<vector<uint8_t>>(_rawArgument->size() -
                                                             (jsonEnd + 1));

    memcpy(aesEncryptedSegment->data(), _rawArgument->data() + jsonEnd + 1, aesEncryptedSegment->size());
}

uint64_t EncryptedArgument::getTimeStamp() const {
    return timeStamp;
}

const ptr<vector<uint8_t>> &EncryptedArgument::getAesEncryptedSegment() const {
    return aesEncryptedSegment;
}

const string &EncryptedArgument::getEncryptedAesKey() const {
    return encryptedAESKey;
}

pair<ptr<AesCbcKeyIVPair>, ptr<vector<uint8_t>>>
EncryptedArgument::generateKeyAndEncryptSegment(ptr<vector<uint8_t>> _plaintextSegment,
                                                        CryptoPP::AutoSeededRandomPool& _prng) {

    auto keyIVPair = make_shared<AesCbcKeyIVPair>(_prng);

    auto encryption = keyIVPair->encrypt(_plaintextSegment);

    return {keyIVPair, encryption};
}
