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

BiteDataField::BiteDataField(const shared_ptr<EncryptedData> &_encryptedKeyPlusData, uint64_t _epoch, bool _doRealCrypto)
    : keyPlusEncryptedData(_encryptedKeyPlusData), epoch(_epoch) {
    CHECK_STATE(_encryptedKeyPlusData);
    CHECK_STATE(_encryptedKeyPlusData->size() > BITE_ENCRYPTED_AES_KEY_LEN);
    
    // get & validate ciphertext + key
    if (_doRealCrypto) {
        // get & validate encrypted key part
        auto aesKey = libBLS::Ciphertext::fromBytes(*keyPlusEncryptedData).key.toBytes();
        auto aesKeyPtr = make_shared<std::array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN>>(aesKey);
        encryptedAESKey = make_shared<EncryptedAESKey>(aesKeyPtr);
    }
    else {
        // Do not validate the key nor the ciphertext, just copy the first BITE_ENCRYPTED_AES_KEY_LEN bytes
        auto keyVec = std::make_shared<std::array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN> >();
        std::copy_n(keyPlusEncryptedData->begin(), BITE_ENCRYPTED_AES_KEY_LEN, keyVec->begin());
        encryptedAESKey = make_shared<EncryptedAESKey>(keyVec);
    }

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

BiteDataField::BiteDataField(const std::shared_ptr<std::vector<uint8_t> > &_data, bool _doRealCrypto) {
    CHECK_STATE(_data);

    // parse RLP-encoded tx data field
    RLPItem rlp(*_data);
    CHECK_STATE2(rlp.isList(), "RLP item is not a list");
    CHECK_STATE2(rlp.size() >= 1, "RLP item should have at least 1 item");
    
    // Get 1st item from list
    RLPItem rlp0 = rlp[0];

    CHECK_STATE2(rlp0.isList(), "RLP item 0 is not a list");
    CHECK_STATE2(rlp0.size() == 2, "RLP item 0 should have exactly 2 fields - EPOCH_ID, and bite encrypted data");

    // validate encrypted data
    keyPlusEncryptedData = make_shared<std::vector<uint8_t>>(rlp0[1].asBytes());
    CHECK_STATE2(keyPlusEncryptedData->size() >= BITE_ENCRYPTED_AES_KEY_LEN,
        "Incorrectly formatted BITE transaction: Encrypted data size is not at least " + to_string(BITE_ENCRYPTED_AES_KEY_LEN) + " bytes, found: " + to_string(keyPlusEncryptedData->size()));
    
    if (_doRealCrypto) {
        // get & validate encrypted key part
        auto aesKey = libBLS::Ciphertext::fromBytes(*keyPlusEncryptedData).key.toBytes();
        auto aesKeyPtr = make_shared<std::array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN>>(aesKey);
        encryptedAESKey = make_shared<EncryptedAESKey>(aesKeyPtr);
    }
    else {
        // Do not validate the key nor the ciphertext, just copy the first BITE_ENCRYPTED_AES_KEY_LEN bytes
        auto keyVec = std::make_shared<std::array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN> >();
        std::copy_n(keyPlusEncryptedData->begin(), BITE_ENCRYPTED_AES_KEY_LEN, keyVec->begin());
        encryptedAESKey = make_shared<EncryptedAESKey>(keyVec);
    }


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


ptr<BiteDataField> BiteDataField::createIfMagicMatches(ptr<vector<uint8_t> > &_data, ptr<vector<uint8_t> > &_to, bool _doRealCrypto) {
    CHECK_STATE(_data)
    CHECK_STATE(_to)

    // compare _to field to BITE magic number
    if (!std::equal(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE,
                    _to->begin())) {
        return nullptr;
    }

    return ptr<BiteDataField>(new BiteDataField(_data, _doRealCrypto));
}



ptr<vector<uint8_t> > &BiteDataField::getSerializedData() {
    return serializedData;
}
