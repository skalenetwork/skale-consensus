//
// Created by kladko on 26.01.22.
//

#ifndef SKALED_AESCBCKEYIVPAIR_H
#define SKALED_AESCBCKEYIVPAIR_H

#include <cryptopp/osrng.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/hmac.h>
#include <cryptopp/hkdf.h>
#include <cryptopp/base64.h>


class AesCbcKeyIVPair {

    CryptoPP::SecByteBlock key;
    ptr<vector<uint8_t>> iv;

public:

    AesCbcKeyIVPair(CryptoPP::AutoSeededRandomPool& _prng);

    AesCbcKeyIVPair(ptr<vector<uint8_t>> _key, ptr<vector<uint8_t>> _iv);



    string toHex();

    ptr<vector<uint8_t>> getKey();

    ptr<vector<uint8_t>> getIV();

    string getIvAsHex();



    ptr<vector<uint8_t>> encrypt(ptr<vector<uint8_t>> _plaintext);

    ptr<vector<uint8_t>> decrypt(ptr<vector<uint8_t>> _ciphertext);

};


#endif //SKALED_AESCBCKEYIVPAIR_H
