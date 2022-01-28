//
// Created by kladko on 26.01.22.
//

#ifndef SKALED_AESKEY_H
#define SKALED_AESKEY_H

#include <cryptopp/osrng.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/hmac.h>
#include <cryptopp/hkdf.h>
#include <cryptopp/base64.h>


class AESKey {

    CryptoPP::SecByteBlock key;

public:

    AESKey(CryptoPP::AutoSeededRandomPool& _prng);

    string toHex();

    ptr<vector<uint8_t>> encrypt(CryptoPP::AutoSeededRandomPool& _prng, ptr<vector<uint8_t>> _plaintext);

    ptr<vector<uint8_t>> decrypt(ptr<vector<uint8_t>> _ciphertext);

};


#endif //SKALED_AESKEY_H
