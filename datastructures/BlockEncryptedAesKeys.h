//
// Created by kladko on 02.02.22.
//

#ifndef SKALED_BLOCKENCRYPTEDAESKEYS_H
#define SKALED_BLOCKENCRYPTEDAESKEYS_H


class BlockEncryptedAesKeys {
    recursive_mutex m;
    ptr<map<uint64_t, string>> encryptedKeys;
public:
    BlockEncryptedAesKeys();
    void add(uint64_t _transactionIndex, const string& _key);
    uint64_t size();

};


#endif //SKALED_BLOCKENCRYPTEDAESKEYS_H
