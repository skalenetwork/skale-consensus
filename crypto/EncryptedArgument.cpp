//
// Created by kladko on 31.01.22.
//

#include "thirdparty/rapidjson/document.h"
#include "thirdparty/json.hpp"
#include "thirdparty/rapidjson/prettywriter.h" // for stringify JSON

#include "SkaleCommon.h"
#include "Log.h"
#include "network/Utils.h"
#include "utils/Time.h"
#include "headers/BasicHeader.h"
#include "AesCbcKeyIVPair.h"

#include "EncryptedArgument.h"

EncryptedArgument::EncryptedArgument(ptr<vector<uint8_t>> _serializedArgument) {
    CHECK_STATE(_serializedArgument);
    CHECK_STATE(_serializedArgument->size() > 10);
    CHECK_STATE(_serializedArgument->front() == '{');
    uint64_t jsonEnd;
    for (jsonEnd = 0; jsonEnd < _serializedArgument->size(); jsonEnd++) {
        if (_serializedArgument->at(jsonEnd) == '}')
            break;
    }

    CHECK_STATE(jsonEnd < _serializedArgument->size() - 5)

    using namespace rapidjson;

    Document d;

    d.Parse((const char*) _serializedArgument->data(), jsonEnd + 1);

    this->serializedArgument = _serializedArgument;
    this->timeStamp = BasicHeader::getUint64Rapid(d, "ts");
    this->encryptedAESKey = BasicHeader::getStringRapid(d, "ek");
    this->iv = BasicHeader::getStringRapid(d, "iv");

    this->aesEncryptedSegment = make_shared<vector<uint8_t>>(_serializedArgument->size() -
                                                             (jsonEnd + 1));
    memcpy(aesEncryptedSegment->data(), _serializedArgument->data() + jsonEnd + 1, aesEncryptedSegment->size());
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

EncryptedArgument::EncryptedArgument(string _encryptedAESKey, ptr<AesCbcKeyIVPair> _plaintextKey,
                                     ptr<vector<uint8_t>> _plaintextArgument) {
    CHECK_STATE(!_encryptedAESKey.empty())
    CHECK_STATE(_plaintextKey);
    CHECK_STATE(_plaintextArgument)
    CHECK_STATE(_plaintextArgument->size() > 0)

    encryptedAESKey = _encryptedAESKey;
    iv = _plaintextKey->getIvAsHex();
    timeStamp = Time::getCurrentTimeMs();

    this->aesEncryptedSegment = _plaintextKey->encrypt(_plaintextArgument);


    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    writer.String("ek");
    writer.String(encryptedAESKey.c_str());
    writer.String("iv");
    writer.String(iv.c_str());
    writer.String("ts");
    writer.Uint64(timeStamp);
    writer.EndObject();
    writer.Flush();
    string header = sb.GetString();
    cerr << header << endl;

    serializedArgument = make_shared<vector<uint8_t>>(header.cbegin(), header.cend());
    serializedArgument->insert(serializedArgument->cend(), aesEncryptedSegment->cbegin(),
                               aesEncryptedSegment->cend());

}

ptr<vector<uint8_t>> EncryptedArgument::serialize() {
    CHECK_STATE(serializedArgument);
    return serializedArgument;
}
