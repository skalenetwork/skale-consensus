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