#include <boost/endian/conversion.hpp>
#include <boost/endian/arithmetic.hpp>


#include "SkaleCommon.h"
#include "Log.h"
#include <crypto/EncryptedAESKey.h>

#include "BiteDataFiled.h"


const auto BITE_HEADER_LEN = BITE_MAGIC_SIZE + sizeof(uint64_t) + BITE_ENCRYPTED_AES_KEY_LEN;

BiteDataField::BiteDataField(const std::shared_ptr<std::vector<uint8_t> > &_data) {
    CHECK_STATE(_data)
    CHECK_STATE(_data->size() >= BITE_MAGIC_SIZE + sizeof(uint64_t) + BITE_ENCRYPTED_AES_KEY_LEN);

    auto keyVec = std::make_shared<std::array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN> >();

    std::copy_n(_data->begin() + BITE_MAGIC_SIZE + sizeof(uint64_t), BITE_ENCRYPTED_AES_KEY_LEN, keyVec->begin());

    auto encryptedDataStart = _data->begin() + BITE_HEADER_LEN;
    encryptedAESKey = make_shared<EncryptedAESKey>(keyVec);
    encryptedData = make_shared<EncryptedData>(encryptedDataStart, _data->end());
    serializedData = _data;
}

ptr<EncryptedAESKey> &BiteDataField::getEncryptedAESKey() {
    return encryptedAESKey;
}

ptr<EncryptedData> &BiteDataField::getEncryptedData() {
    CHECK_STATE(encryptedData)
    return encryptedData;
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

BiteDataField::BiteDataField(const ptr<EncryptedAESKey>& _encryptedAESKey,
                             const shared_ptr<EncryptedData> &_encryptedData, uint64_t _epoch)
    : encryptedAESKey(_encryptedAESKey), encryptedData(_encryptedData), epoch(_epoch) {
    CHECK_STATE(_encryptedData);
    CHECK_STATE(_encryptedAESKey);

    serializedData = make_shared<vector<uint8_t> >();
    serializedData->reserve(BITE_HEADER_LEN + _encryptedData->size());
    serializedData->insert(serializedData->end(), BITE_MAGIC_AS_BYTE_ARRAY,BITE_MAGIC_AS_BYTE_ARRAY + BITE_MAGIC_SIZE);
    uint64_t epochBE = boost::endian::native_to_big(_epoch);
    uint8_t* epochBytes = reinterpret_cast<uint8_t*>(&epochBE);
    serializedData->insert(serializedData->end(), epochBytes, epochBytes + sizeof(epochBE));
    serializedData->insert(serializedData->end(), encryptedAESKey->getKey()->begin(), encryptedAESKey->getKey()->end());
    CHECK_STATE(serializedData->size() == BITE_HEADER_LEN);
    serializedData->insert(serializedData->end(), encryptedData->begin(), encryptedData->end());
}

ptr<vector<uint8_t> > &BiteDataField::getSerializedData() {
    return serializedData;
}
