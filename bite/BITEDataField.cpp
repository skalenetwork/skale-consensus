
#include "SkaleCommon.h"
#include "Log.h"
#include "BITEDataFiled.h"


BITEDataField::BITEDataField( const std::shared_ptr< std::vector< uint8_t > >& _data ) {
    CHECK_STATE( _data && _data->size() >= BITE_MAGIC_SIZE + ENCRYPTED_AES_KEY_LEN );
    std::copy_n(
        _data->begin() + BITE_MAGIC_SIZE, ENCRYPTED_AES_KEY_LEN, encryptedAESKey.begin() );

    auto encryptedDataStart = _data->begin() + BITE_MAGIC_SIZE + ENCRYPTED_AES_KEY_LEN;
    encryptedData = std::make_shared< EncryptedData >( encryptedDataStart, _data->end() );
}
const EncryptedAESKey& BITEDataField::getEncryptedAESKey() const {
    return encryptedAESKey;
}
const ptr< EncryptedData >& BITEDataField::getEncryptedData() const {
    return encryptedData;
}
uint64_t BITEDataField::getEpoch() const {
    return epoch;
}

ptr< BITEDataField > BITEDataField::createIfMagicMatches( ptr< vector< uint8_t > >& _data ) {
    CHECK_STATE( _data )
    if ( _data->size() < BITE_MAGIC_SIZE + ENCRYPTED_AES_KEY_LEN )
        return nullptr;

    if ( !std::equal( BITE_MAGIC_AS_BYTE_ARRAY, BITE_MAGIC_AS_BYTE_ARRAY + BITE_MAGIC_SIZE,
             _data->begin() ) ) {
        return nullptr;
    }

    return ptr<BITEDataField>(new BITEDataField(_data ));
}
