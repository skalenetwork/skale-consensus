//
// Created by kladko on 31.01.22.
//

#ifndef SKALED_ENCRYPTEDARGUMENT_H
#define SKALED_ENCRYPTEDARGUMENT_H

namespace CryptoPP {
    class AutoSeededRandomPool;
}

class AesCbcKeyIVPair;

class EncryptedArgument {

    uint64_t timeStamp;
    string encryptedAESKey;
    string iv;
    ptr<vector<uint8_t>> aesEncryptedSegment;

    ptr<vector<uint8_t>> serializedArgument = nullptr;

public:
    const string &getEncryptedAesKey() const;

    ptr<vector<uint8_t>> serialize();

    EncryptedArgument(string _encryptedAESKey, ptr<AesCbcKeyIVPair> _plaintextKey,
                                         ptr<vector<uint8_t>> _plaintextArgument);


    EncryptedArgument(ptr<vector<uint8_t>> _serializedArgument);

    uint64_t getTimeStamp() const;

    const ptr<vector<uint8_t>> &getAesEncryptedSegment() const;

    static pair<ptr<AesCbcKeyIVPair>, ptr<vector<uint8_t>>> generateKeyAndEncryptSegment(
            ptr<vector<uint8_t>> _plaintextSegment, CryptoPP::AutoSeededRandomPool& _prng);


};


#endif //SKALED_ENCRYPTEDARGUMENT_H
