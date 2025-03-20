
#include "SkaleCommon.h"
#include "Log.h"

#include "network/Utils.h"

#include "BLAKE3Hash.h"
#include "AES256Key.h"

void AES256Key::print() {
    for ( size_t i = 0; i < AES256_KEY_LEN; i++ ) {
        cerr << to_string( aes256Key.at( i ) );
    }
}


uint8_t AES256Key::at( uint32_t _position ) {
    return aes256Key.at( _position );
}


AES256Key AES256Key::fromHex( const string& _hex ) {
    CHECK_ARGUMENT( _hex != "" );
    AES256Key result;
    Utils::cArrayFromHex( _hex, result.data(), HASH_LEN );
    return result;
}

string AES256Key::toHex() {
    auto result = Utils::carray2Hex( aes256Key.data(), AES256_KEY_LEN );
    CHECK_STATE( result != "" );
    return result;
}


int AES256Key::compare( AES256Key& _key2 ) {
    for ( size_t i = 0; i < HASH_LEN; i++ ) {
        if ( aes256Key.at( i ) < _key2.at( i ) )
            return -1;
        if ( aes256Key.at( i ) > _key2.at( i ) )
            return 1;
    }
    return 0;
}


BLAKE3Hash AES256Key::calculateHash( const ptr< vector< uint8_t > >& _data ) {
    CHECK_ARGUMENT( _data );
    // Initialize the hasher.

    blake3_hasher hasher;
    blake3_hasher_init( &hasher );
    blake3_hasher_update( &hasher, _data->data(), _data->size() );
    BLAKE3Hash hash;
    blake3_hasher_finalize( &hasher, hash.data(), BLAKE3_OUT_LEN );
    return hash;
}


const array< uint8_t, HASH_LEN >& AES256Key::getKey() const {
    return aes256Key;
}

