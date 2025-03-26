#include <random>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include "SkaleCommon.h"
#include "Log.h"
#include "node/ConsensusInterface.h"
#include "BLAKE3Hash.h"
#include "network/Utils.h"
#include "EncryptedAESKey.h"
#include "DecryptedAESKey.h"

DecryptedAESKey::~DecryptedAESKey() {}


void DecryptedAESKey::print() {
    for ( size_t i = 0; i < BITE_AES_KEY_LEN; i++ ) {
        cerr << to_string( aesKey.at( i ) );
    }
}


uint8_t DecryptedAESKey::at( uint32_t _position ) {
    return aesKey.at( _position );
}

string DecryptedAESKey::toHex() {
    auto result = Utils::carray2Hex( aesKey.data(), BITE_AES_KEY_LEN );
    CHECK_STATE( result != "" );
    return result;
}


int DecryptedAESKey::compare( DecryptedAESKey& _key2 ) {
    for ( size_t i = 0; i < HASH_LEN; i++ ) {
        if ( aesKey.at( i ) < _key2.at( i ) )
            return -1;
        if ( aesKey.at( i ) > _key2.at( i ) )
            return 1;
    }
    return 0;
}


ptr<EncryptedAESKey> DecryptedAESKey::generate() {
    auto randomKey = std::make_shared<std::array<std::uint8_t, BITE_ENCRYPTED_AES_KEY_LEN>>();
    // thread safe permanent objects
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    thread_local  std::uniform_int_distribution<std::uint8_t> dis(0, 255);

    for (auto& byte : *randomKey) {
        byte = dis(gen);
    }

    return make_shared<EncryptedAESKey>(randomKey);
}


ptr<vector<uint8_t>> DecryptedAESKey::encryptData(ptr<vector<uint8_t>>& _data)  {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP_CIPHER_CTX");
    }

    std::shared_ptr<std::vector<uint8_t>> encryptedData = std::make_shared<std::vector<uint8_t>>(_data->size() + EVP_MAX_BLOCK_LENGTH);
    std::array<uint8_t, EVP_MAX_IV_LENGTH> iv;
    if (!RAND_bytes(iv.data(), iv.size())) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to generate IV");
    }

    int len;
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, aesKey.data(), iv.data())) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize encryption");
    }

    if (1 != EVP_EncryptUpdate(ctx, encryptedData->data(), &len, _data->data(), _data->size())) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to encrypt data");
    }
    int ciphertext_len = len;

    if (1 != EVP_EncryptFinal_ex(ctx, encryptedData->data() + len, &len)) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to finalize encryption");
    }
    ciphertext_len += len;

    encryptedData->resize(ciphertext_len);
    encryptedData->insert(encryptedData->begin(), iv.begin(), iv.end());

    EVP_CIPHER_CTX_free(ctx);
    return encryptedData;
}


std::shared_ptr<std::vector<uint8_t>> DecryptedAESKey::decryptData(ptr<std::vector<uint8_t>>& encryptedData)  {
    if (encryptedData->size() < EVP_MAX_IV_LENGTH) {
        throw std::runtime_error("Invalid encrypted data");
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP_CIPHER_CTX");
    }

    std::array<uint8_t, EVP_MAX_IV_LENGTH> iv;
    std::copy(encryptedData->begin(), encryptedData->begin() + iv.size(), iv.begin());

    std::shared_ptr<std::vector<uint8_t>> decryptedData = std::make_shared<std::vector<uint8_t>>(encryptedData->size() - iv.size());

    int len;
    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, aesKey.data(), iv.data())) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize decryption");
    }

    if (1 != EVP_DecryptUpdate(ctx, decryptedData->data(), &len, encryptedData->data() + iv.size(), encryptedData->size() - iv.size())) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to decrypt data");
    }
    int plaintext_len = len;

    if (1 != EVP_DecryptFinal_ex(ctx, decryptedData->data() + len, &len)) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to finalize decryption");
    }
    plaintext_len += len;

    decryptedData->resize(plaintext_len);

    EVP_CIPHER_CTX_free(ctx);
    return decryptedData;
}