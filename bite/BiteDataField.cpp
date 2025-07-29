#include <boost/endian/conversion.hpp>
#include <boost/endian/arithmetic.hpp>


#include "SkaleCommon.h"
#include "Log.h"
#include <crypto/EncryptedAESKey.h>

#include "bite/BiteDataFiled.h"
#include "rlp/RLPStream.h"
#include "rlp/RLP.h"

/// Minimum size of BITE field excluding the ciphertext from libBLS
/// which includes both the key + ciphered data
const auto BITE_MIN_DATA_LEN = BITE_EPOCH_ID_LEN + ADDRESS_SIZE;

BiteDataField::BiteDataField(const shared_ptr<EncryptedData> &_encryptedKeyPlusData, uint64_t _epoch)
    : keyPlusEncryptedData(_encryptedKeyPlusData), epoch(_epoch) {
    CHECK_STATE(_encryptedKeyPlusData);
    CHECK_STATE(_encryptedKeyPlusData->size() > BITE_ENCRYPTED_AES_KEY_LEN);
    

    // Do not validate the key nor the ciphertext, just copy the first BITE_ENCRYPTED_AES_KEY_LEN bytes
    auto keyVec = std::make_shared<std::array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN> >();
    std::copy_n(keyPlusEncryptedData->begin(), BITE_ENCRYPTED_AES_KEY_LEN, keyVec->begin());
    encryptedAESKey = make_shared<EncryptedAESKey>(keyVec);


    // build serialized RLP-encoded data field
    uint64_t epochBE = boost::endian::native_to_big(_epoch);
    std::vector<uint8_t> epochBytes(reinterpret_cast<uint8_t*>(&epochBE),
                                reinterpret_cast<uint8_t*>(&epochBE) + sizeof(epochBE));

    RLPStream list;
    list << epochBytes << *_encryptedKeyPlusData;

    RLPStream listOfLists;
    listOfLists << list;

    serializedData = make_shared<vector<uint8_t> >(listOfLists.encode());
}

BiteDataField::BiteDataField(const std::shared_ptr<std::vector<uint8_t> > &_data, u256 _currentEpochId) {
    CHECK_STATE(_data);

    // parse RLP-encoded tx data field
    // RLP structure: [[epochId1, encryptedAESKey1],[epochId2, encryptedAESKey2], encryptedData]
    // where [epochId2, encryptedAESKey2] is an optional element
    RLPItem rlp(*_data);
    CHECK_STATE2(rlp.isList(), "RLP item is not a list");
    CHECK_STATE2(rlp.size() > 0, "RLP item should have at least 1 item");
    CHECK_STATE2(rlp.size() < 4, "RLP item should not have more than 3 items");
    
    const uint64_t currentEpoch = _currentEpochId.convert_to<uint64_t>();

    // encrypted data always goes last
    const auto encryptedData = make_shared<std::vector<uint8_t>>(rlp[rlp.size() - 1].asBytes());
    
    auto parseRLPItem = [](const RLPItem& item) {
        CHECK_STATE2(item.isList(), "RLP item is not a list");
        CHECK_STATE2(item.size() == 2, "RLP item should have exactly 2 fields - EPOCH_ID, and bite encrypted data");

        // Parse epoch ID
        const auto epochIdBytes = item[0].asBytes();
        CHECK_STATE2(epochIdBytes.size() <= sizeof(uint64_t), "Epoch id too long");
        const uint64_t parsedEpoch = u256(epochIdBytes).convert_to<uint64_t>();

        // Parse and validate encrypted data
        auto encryptedAESKey = make_shared<std::vector<uint8_t>>(item[1].asBytes());
        CHECK_STATE2(encryptedAESKey->size() >= BITE_ENCRYPTED_AES_KEY_LEN,
            "Incorrectly formatted BITE transaction: Encrypted AES key size is not at least " +
            to_string(BITE_ENCRYPTED_AES_KEY_LEN) + " bytes, found: " + to_string(encryptedAESKey->size()));

        return std::make_pair(parsedEpoch, std::move(encryptedAESKey));
    };

    // Parse first RLP item
    auto [firstEpoch, firstEncryptedAESKey] = parseRLPItem(rlp[0]);
    epoch = firstEpoch;

    std::shared_ptr<std::vector<uint8_t>> selectedEncryptedAESKey;

    // If there's a second item and epochId doesn't match, use the second item
    if (rlp.size() >= 2 && epoch != currentEpoch) {
        auto [secondEpoch, secondEncryptedAESKey] = parseRLPItem(rlp[1]);
        epoch = secondEpoch;
        selectedEncryptedAESKey = secondEncryptedAESKey;
    } else {
        selectedEncryptedAESKey = firstEncryptedAESKey;
    }
    
    CHECK_STATE2(currentEpoch == epoch, "Incorrectly formatted BITE transaction: wrong epochId");
    
    // Create array from the first BITE_ENCRYPTED_AES_KEY_LEN bytes
    auto encryptedAESKeyRawBytes = make_shared<std::array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN>>();
    std::copy_n(selectedEncryptedAESKey->begin(), BITE_ENCRYPTED_AES_KEY_LEN, encryptedAESKeyRawBytes->begin());
    encryptedAESKey = make_shared<EncryptedAESKey>(encryptedAESKeyRawBytes);

    // Combine selectedEncryptedAESKey + encryptedData
    keyPlusEncryptedData = make_shared<std::vector<uint8_t>>();
    keyPlusEncryptedData->reserve(selectedEncryptedAESKey->size() + encryptedData->size());
    keyPlusEncryptedData->insert(keyPlusEncryptedData->end(), selectedEncryptedAESKey->begin(), selectedEncryptedAESKey->end());
    keyPlusEncryptedData->insert(keyPlusEncryptedData->end(), encryptedData->begin(), encryptedData->end());

    serializedData = _data;
}

ptr<EncryptedAESKey> &BiteDataField::getEncryptedAESKey() {
    return encryptedAESKey;
}
const shared_ptr< EncryptedData >& BiteDataField::getKeyPlusEncryptedData() const {
    return keyPlusEncryptedData;
}

uint64_t BiteDataField::getEpoch() {
    return epoch;
}


ptr<BiteDataField> BiteDataField::createIfMagicMatches(ptr<vector<uint8_t> > &_data,
                                                       ptr<vector<uint8_t> > &_to,
                                                       u256 _currentEpochId) {
    CHECK_STATE(_data)
    CHECK_STATE(_to)

    // compare _to field to BITE magic number
    if (!std::equal(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE,
                    _to->begin())) {
        return nullptr;
    }

    return ptr<BiteDataField>(new BiteDataField(_data, _currentEpochId));
}



ptr<vector<uint8_t> > &BiteDataField::getSerializedData() {
    return serializedData;
}
