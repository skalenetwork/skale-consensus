//
// Created by kladko on 26.01.22.
//

#ifndef SKALED_AESCBCKEY_H
#define SKALED_AESCBCKEY_H

#include <cryptopp/osrng.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/hmac.h>
#include <cryptopp/hkdf.h>
#include <cryptopp/base64.h>


class AesCBCKey {

    CryptoPP::SecByteBlock key;
    ptr<vector<uint8_t>> iv;

public:

    AesCBCKey(CryptoPP::AutoSeededRandomPool& _prng);

    AesCBCKey(ptr<vector<uint8_t>> _key, ptr<vector<uint8_t>> _iv);



    string toHex();

    ptr<vector<uint8_t>> encrypt(ptr<vector<uint8_t>> _plaintext);

    ptr<vector<uint8_t>> decrypt(ptr<vector<uint8_t>> _ciphertext);

};


#endif //SKALED_AESCBCKEY_H
