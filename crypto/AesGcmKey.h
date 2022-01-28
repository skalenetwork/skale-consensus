//
// Created by kladko on 26.01.22.
//

#ifndef SKALED_AESGCMKEY_H
#define SKALED_AESGCMKEY_H

#include <cryptopp/osrng.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/hmac.h>
#include <cryptopp/hkdf.h>
#include <cryptopp/base64.h>


class AesGcmKey {

    CryptoPP::SecByteBlock key;

public:

    AesGcmKey(CryptoPP::AutoSeededRandomPool& _prng);

    string toHex();

    ptr<vector<uint8_t>> encrypt(CryptoPP::AutoSeededRandomPool& _prng, ptr<vector<uint8_t>> _plaintext);

    ptr<vector<uint8_t>> decrypt(ptr<vector<uint8_t>> _ciphertext);

};


#endif //SKALED_AESGCMKEY_H
