#include <boost/endian/conversion.hpp>
#include <boost/endian/arithmetic.hpp>


#include "SkaleCommon.h"
#include "Log.h"
#include <crypto/EncryptedAESKey.h>

#include "BITEDataFiled.h"


BITEDataField::BITEDataField(const std::shared_ptr<std::vector<uint8_t> > &_data) {
    CHECK_STATE(_data && _data->size() >= BITE_MAGIC_SIZE + BITE_ENCRYPTED_AES_KEY_LEN);

    auto keyVec = std::make_shared<std::array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN> >();

    std::copy_n(_data->begin() + BITE_MAGIC_SIZE, BITE_ENCRYPTED_AES_KEY_LEN, keyVec->begin());

    auto encryptedDataStart = _data->begin() + BITE_MAGIC_SIZE + BITE_ENCRYPTED_AES_KEY_LEN;
    encryptedData = std::make_shared<EncryptedData>(encryptedDataStart, _data->end());
    serializedData = _data;
}

ptr<EncryptedAESKey> &BITEDataField::getEncryptedAESKey() {
    return encryptedAESKey;
}

ptr<EncryptedData> &BITEDataField::getEncryptedData() {
    CHECK_STATE(encryptedData)
    return encryptedData;
}

uint64_t BITEDataField::getEpoch() {
    return epoch;
}

ptr<BITEDataField> BITEDataField::createIfMagicMatches(ptr<vector<uint8_t> > &_data) {
    CHECK_STATE(_data)
    if (_data->size() < BITE_MAGIC_SIZE + BITE_ENCRYPTED_AES_KEY_LEN)
        return nullptr;

    if (!std::equal(BITE_MAGIC_AS_BYTE_ARRAY, BITE_MAGIC_AS_BYTE_ARRAY + BITE_MAGIC_SIZE,
                    _data->begin())) {
        return nullptr;
    }

    return ptr<BITEDataField>(new BITEDataField(_data));
}

BITEDataField::BITEDataField(const ptr<EncryptedAESKey>& _encryptedAESKey,
                             const shared_ptr<EncryptedData> &_encryptedData, uint64_t _epoch)
    : encryptedAESKey(_encryptedAESKey), encryptedData(_encryptedData), epoch(_epoch) {
    CHECK_STATE(_encryptedData);
    CHECK_STATE(_encryptedAESKey);

    serializedData = make_shared<vector<uint8_t> >();
    serializedData->reserve(
        BITE_MAGIC_SIZE + BITE_ENCRYPTED_AES_KEY_LEN + _encryptedData->size());
    serializedData->insert(serializedData->end(), BITE_MAGIC_AS_BYTE_ARRAY,
                           BITE_MAGIC_AS_BYTE_ARRAY + BITE_MAGIC_SIZE);
    serializedData->insert(serializedData->end(), encryptedAESKey->getKey()->begin(), encryptedAESKey->getKey()->end());
    serializedData->insert(serializedData->end(), encryptedData->begin(), encryptedData->end());
}

ptr<vector<uint8_t> > &BITEDataField::getSerializedData() {
    return serializedData;
}
