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
    static ptr<AESKey> generateKey();

};


#endif //SKALED_AESKEY_H
