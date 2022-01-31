//
// Created by kladko on 31.01.22.
//

#ifndef SKALED_ENCRYPTEDARGUMENT_H
#define SKALED_ENCRYPTEDARGUMENT_H


class EncryptedArgument {

    uint64_t timeStamp;
    string encryptedAESKey;

public:
    EncryptedArgument(ptr<vector<uint8_t>> _rawArgument);


};


#endif //SKALED_ENCRYPTEDARGUMENT_H
