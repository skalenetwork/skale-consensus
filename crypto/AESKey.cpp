
#include "SkaleCommon.h"
#include "Log.h"

#include "network/Utils.h"

#include "BLAKE3Hash.h"
#include "AESKey.h"

void AESKey::print() {
    for ( size_t i = 0; i < AES_KEY_LEN; i++ ) {
        cerr << to_string( aesKey.at( i ) );
    }
}


uint8_t AESKey::at( uint32_t _position ) {
    return aesKey.at( _position );
}


AESKey AESKey::fromHex( const string& _hex ) {
    CHECK_ARGUMENT( _hex != "" );
    AESKey result;
    Utils::cArrayFromHex( _hex, result.data(), HASH_LEN );
    return result;
}

string AESKey::toHex() {
    auto result = Utils::carray2Hex( aesKey.data(), AES_KEY_LEN );
    CHECK_STATE( result != "" );
    return result;
}


int AESKey::compare( AESKey& _key2 ) {
    for ( size_t i = 0; i < HASH_LEN; i++ ) {
        if ( aesKey.at( i ) < _key2.at( i ) )
            return -1;
        if ( aesKey.at( i ) > _key2.at( i ) )
            return 1;
    }
    return 0;
}


BLAKE3Hash AESKey::calculateHash( const ptr< vector< uint8_t > >& _data ) {
    CHECK_ARGUMENT( _data );
    // Initialize the hasher.

    blake3_hasher hasher;
    blake3_hasher_init( &hasher );
    blake3_hasher_update( &hasher, _data->data(), _data->size() );
    BLAKE3Hash hash;
    blake3_hasher_finalize( &hasher, hash.data(), BLAKE3_OUT_LEN );
    return hash;
}


const array< uint8_t, HASH_LEN >& AESKey::getKey() const {
    return aesKey;
}

