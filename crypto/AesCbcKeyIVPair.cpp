//
// Created by kladko on 26.01.22.
//


#include <cryptopp/eccrypto.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#include <cryptopp/oids.h>
#include <cryptopp/hex.h>
#include <cryptopp/gcm.h>

#include "SkaleCommon.h"
#include "Log.h"

#include "network/Utils.h"
#include "AesCbcKeyIVPair.h"



AesCbcKeyIVPair::AesCbcKeyIVPair(CryptoPP::AutoSeededRandomPool& _prng) : key(AES_KEY_LEN ) {
    _prng.GenerateBlock( key, key.size() );
    iv = make_shared<vector<uint8_t>>(CryptoPP::AES::BLOCKSIZE);
    _prng.GenerateBlock(iv->data(), iv->size());
}

AesCbcKeyIVPair::AesCbcKeyIVPair(ptr<vector<uint8_t>> _key, ptr<vector<uint8_t>> _iv) : key(_key->data(), _key->size()) {
    CHECK_STATE(_iv)
    iv = _iv;
}

ptr<vector<uint8_t>> AesCbcKeyIVPair::encrypt(ptr<vector<uint8_t>> _plaintext) {

    using namespace CryptoPP;

    try {
        CBC_Mode< AES >::Encryption e;
        e.SetKeyWithIV( key, key.size(), iv->data(), CryptoPP::AES::BLOCKSIZE );
        string cipher;

        StringSource((CryptoPP::byte*) _plaintext->data(), _plaintext->size(), true,
                     new StreamTransformationFilter( e, new StringSink( cipher )));

        CHECK_STATE(cipher.size() > 0);
        auto result = make_shared<vector<uint8_t>>(cipher.size());
        std::memcpy(result->data(), cipher.c_str(), cipher.size());
        return result;
    } catch (exception& e) {
        LOG(err, "Could not  GCM encrypt:" + string(e.what()));
        throw_with_nested(InvalidStateException(__FUNCTION__ , __CLASS_NAME__));
    }
}


ptr<vector<uint8_t>> AesCbcKeyIVPair::decrypt(
        ptr<vector<uint8_t>> _ciphertext) {

    using namespace CryptoPP;

    try {
        CBC_Mode< AES >::Decryption d;

        d.SetKeyWithIV( key, key.size(), iv->data(), iv->size() );

        string cipher((char*)_ciphertext->data(), _ciphertext->size());

        string plaintext;


        // The StreamTransformationFilter removes
        //  padding as required.
        StringSource ss( cipher, true,
                         new StreamTransformationFilter( d,
                                                         new StringSink(plaintext )
                         ) // StreamTransformationFilter
        ); // StringSource


        CHECK_STATE(cipher.size() > 0);
        auto result = make_shared<vector<uint8_t>>(plaintext.size());
        std::memcpy(result->data(), plaintext.c_str(), plaintext.size());
        return result;

    } catch (exception& e) {
        LOG(err, "Could not  AES-CBC decrypt:" + string(e.what()));
        throw_with_nested(InvalidStateException(__FUNCTION__ , __CLASS_NAME__));
    }
}

ptr<vector<uint8_t>> AesCbcKeyIVPair::getKey() {
    auto res =  make_shared<vector<uint8_t>>(key.size());
    memcpy(res->data(), key.data(), key.size());
    return res;
}

ptr<vector<uint8_t>> AesCbcKeyIVPair::getIV() {
    CHECK_STATE(iv);
    return iv;
}

string AesCbcKeyIVPair::getIvAsHex() {
    CHECK_STATE(iv)
    return Utils::carray2Hex(iv->data(), iv->size());
}


