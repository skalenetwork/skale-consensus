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
#include "AESKey.h"



AESKey::AESKey(CryptoPP::AutoSeededRandomPool& _prng) : key(CryptoPP::AES::DEFAULT_KEYLENGTH ) {
    _prng.GenerateBlock( key, key.size() );
}

ptr<vector<uint8_t>> AESKey::encrypt(CryptoPP::AutoSeededRandomPool& _prng, ptr<vector<uint8_t>> _plaintext) {

    using namespace CryptoPP;

    try {
        CryptoPP::byte iv[AES::BLOCKSIZE];
        _prng.GenerateBlock( iv, sizeof(iv) );
        GCM< AES >::Encryption e;
        e.SetKeyWithIV( key, key.size(), iv, sizeof(iv) );

        string cipher;

        StringSource((CryptoPP::byte*) _plaintext->data(), _plaintext->size(), true,
                     new AuthenticatedEncryptionFilter( e, new StringSink( cipher ),
         false, AES_GCM_TAG_SIZE));

        CHECK_STATE(cipher.size() > 0);
        auto result = make_shared<vector<uint8_t>>(cipher.size());
        std::memcpy(result->data(), cipher.c_str(), cipher.size());
        return result;

    } catch (exception& e) {
        LOG(err, "Could not  GCM encrypt:" + string(e.what()));
        throw_with_nested(InvalidStateException(__FUNCTION__ , __CLASS_NAME__));
    }
}
