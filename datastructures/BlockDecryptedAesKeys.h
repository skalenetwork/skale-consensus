//
// Created by kladko on 01.02.22.
//

#ifndef SKALED_BLOCKDECRYPTEDAESKEYS_H
#define SKALED_BLOCKDECRYPTEDAESKEYS_H


class BlockEncryptedArguments;

class BlockDecryptedAesKeys {
    ptr<map<uint64_t, ptr<vector<uint8_t>>>> decryptedAesKeys;
public:
    BlockDecryptedAesKeys();

    explicit BlockDecryptedAesKeys(ptr<map<uint64_t, ptr<vector<uint8_t>>>> _decryptedAesKeys);

    explicit BlockDecryptedAesKeys(ptr<map<uint64_t, string>> _decryptedAesKeys);

    ptr<map<uint64_t, ptr<vector<uint8_t>>>> decryptArgs(ptr<BlockEncryptedArguments> _args);
};


#endif //SKALED_BLOCKDECRYPTEDAESKEYS_H
