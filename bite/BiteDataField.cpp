#include <boost/endian/conversion.hpp>
#include <boost/endian/arithmetic.hpp>


#include "SkaleCommon.h"
#include "Log.h"
#include <crypto/EncryptedAESKey.h>

#include "BiteDataFiled.h"


const auto BITE_HEADER_LEN = BITE_MAGIC_SIZE + BITE_EPOCH_ID_LEN + BITE_ENCRYPTED_AES_KEY_LEN;

BiteDataField::BiteDataField(const shared_ptr<EncryptedData> &_encryptedKeyPlusData, uint64_t _epoch)
    : encryptedKeyPlusData(_encryptedKeyPlusData), epoch(_epoch) {
    CHECK_STATE(_encryptedKeyPlusData);
    CHECK_STATE(_encryptedKeyPlusData->size() > BITE_ENCRYPTED_AES_KEY_LEN);

    auto aesKeyArray = std::make_shared<std::array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN>>();
    std::copy_n(_encryptedKeyPlusData->begin(), BITE_ENCRYPTED_AES_KEY_LEN, aesKeyArray->begin());
    encryptedAESKey = std::make_shared<EncryptedAESKey>(aesKeyArray);
    serializedData = make_shared<vector<uint8_t> >();
    serializedData->reserve(BITE_HEADER_LEN + _encryptedKeyPlusData->size());
    serializedData->insert(serializedData->end(), BITE_MAGIC_AS_BYTE_ARRAY,BITE_MAGIC_AS_BYTE_ARRAY + BITE_MAGIC_SIZE);
    uint64_t epochBE = boost::endian::native_to_big(_epoch);
    uint8_t* epochBytes = reinterpret_cast<uint8_t*>(&epochBE);
    serializedData->insert(serializedData->end(), epochBytes, epochBytes + sizeof(epochBE));
    serializedData->insert(serializedData->end(), encryptedKeyPlusData->begin(), encryptedKeyPlusData->end());
}

BiteDataField::BiteDataField(const std::shared_ptr<std::vector<uint8_t> > &_data) {
    CHECK_STATE(_data)
    CHECK_STATE(_data->size() >= BITE_MAGIC_SIZE + sizeof(uint64_t) + BITE_ENCRYPTED_AES_KEY_LEN);

    auto keyVec = std::make_shared<std::array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN> >();

    std::copy_n(_data->begin() + BITE_MAGIC_SIZE + sizeof(uint64_t), BITE_ENCRYPTED_AES_KEY_LEN, keyVec->begin());

    auto encryptedDataStart = _data->begin() + BITE_HEADER_LEN;
    encryptedAESKey = make_shared<EncryptedAESKey>(keyVec);
    encryptedKeyPlusData = make_shared<EncryptedData>(encryptedDataStart, _data->end());
    keyPlusEncryptedData = make_shared<vector<uint8_t>>(_data->begin() + BITE_MAGIC_SIZE + BITE_EPOCH_ID_LEN,
        _data->end());
    serializedData = _data;
}

ptr<EncryptedAESKey> &BiteDataField::getEncryptedAESKey() {
    return encryptedAESKey;
}
const shared_ptr< EncryptedData >& BiteDataField::getKeyPlusEncryptedData() const {
    return keyPlusEncryptedData;
}

ptr<EncryptedData> &BiteDataField::getEncryptedData() {
    CHECK_STATE(encryptedKeyPlusData)
    return encryptedKeyPlusData;
}

uint64_t BiteDataField::getEpoch() {
    return epoch;
}



ptr<BiteDataField> BiteDataField::createIfMagicMatches(ptr<vector<uint8_t> > &_data) {


    CHECK_STATE(_data)

    if (_data->size() < BITE_MAGIC_SIZE)
        return nullptr;

    if (!std::equal(BITE_MAGIC_AS_BYTE_ARRAY, BITE_MAGIC_AS_BYTE_ARRAY + BITE_MAGIC_SIZE,
                    _data->begin())) {
        return nullptr;
    }

    CHECK_STATE2 (_data->size() >= BITE_HEADER_LEN,
        "Icorrectly formattted BITE transaction: Dsta size too short" + to_string(_data->size()));

    return ptr<BiteDataField>(new BiteDataField(_data));
}



ptr<vector<uint8_t> > &BiteDataField::getSerializedData() {
    return serializedData;
}
