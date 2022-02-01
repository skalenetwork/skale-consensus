//
// Created by kladko on 31.01.22.
//

#ifndef SKALED_ENCRYPTEDARGUMENT_H
#define SKALED_ENCRYPTEDARGUMENT_H


class EncryptedArgument {

    uint64_t timeStamp;
    string encryptedAESKey;
    ptr<vector<uint8_t>> aesEncryptedArg;
public:
    const string &getEncryptedAesKey() const;

public:
    EncryptedArgument(ptr<vector<uint8_t>> _rawArgument);

    uint64_t getTimeStamp() const;

    const ptr<vector<uint8_t>> &getAesEncryptedArg() const;


};


#endif //SKALED_ENCRYPTEDARGUMENT_H
