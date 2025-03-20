
#include "SkaleCommon.h"
#include "Log.h"
#include "BLAKE3Hash.h"

#include "network/Utils.h"

#include "DecryptedAESKey.h"

block_id DecryptedAESKey::getBlockId() const {
    return blockId;
}

DecryptedAESKey::~DecryptedAESKey() {}

DecryptedAESKey::DecryptedAESKey( const string& _key, const block_id& blockId,
    uint64_t _totalDecryptors, uint64_t _requiredDecryptors )
    : blockId( blockId ),
      totalDecryptors( _totalDecryptors ),
      requiredDecryptors( _requiredDecryptors ) {
    CHECK_STATE( _key.size() == AES_KEY_LEN * 2 )

    Utils::cArrayFromHex( _key, this->aesKey.data(), AES_KEY_LEN );
}


void DecryptedAESKey::print() {
    for ( size_t i = 0; i < AES_KEY_LEN; i++ ) {
        cerr << to_string( aesKey.at( i ) );
    }
}


uint8_t DecryptedAESKey::at( uint32_t _position ) {
    return aesKey.at( _position );
}

string DecryptedAESKey::toHex() {
    auto result = Utils::carray2Hex( aesKey.data(), AES_KEY_LEN );
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


BLAKE3Hash DecryptedAESKey::calculateHash( const ptr< vector< uint8_t > >& _data ) {
    CHECK_ARGUMENT( _data );
    // Initialize the hasher.

    blake3_hasher hasher;
    blake3_hasher_init( &hasher );
    blake3_hasher_update( &hasher, _data->data(), _data->size() );
    BLAKE3Hash hash;
    blake3_hasher_finalize( &hasher, hash.data(), BLAKE3_OUT_LEN );
    return hash;
}
