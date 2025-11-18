#include <boost/endian/conversion.hpp>
#include <boost/endian/arithmetic.hpp>


#include "SkaleCommon.h"
#include "Log.h"
#include <crypto/EncryptedAESKey.h>

#include "bite/BiteCiphertext.h"
#include "rlp/RLPStream.h"
#include "rlp/RLP.h"

/// Minimum size of BITE field excluding the ciphertext from libBLS
/// which includes both the key + ciphered data
const auto BITE_MIN_DATA_LEN = BITE_EPOCH_ID_LEN + ADDRESS_SIZE;
const auto KEY_COUNT_BYTE_OFFSET = 1;


BiteCiphertext::BiteCiphertext(const shared_ptr<EncryptedData> &_encryptedKeyPlusData, uint64_t _epoch)
    : keyPlusEncryptedData(_encryptedKeyPlusData), epoch(_epoch) {
    CHECK_STATE(_encryptedKeyPlusData);
    CHECK_STATE(_encryptedKeyPlusData->size() > BITE_ENCRYPTED_AES_KEY_LEN);
    

    // Do not validate the key nor the ciphertext, just copy the first BITE_ENCRYPTED_AES_KEY_LEN bytes
    auto keyVec = std::array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN>();
    std::copy_n(keyPlusEncryptedData->begin(), BITE_ENCRYPTED_AES_KEY_LEN, keyVec.begin());
    encryptedAESKey.emplace(EncryptedAESKey(keyVec));


    // build serialized RLP-encoded data field
    uint64_t epochBE = boost::endian::native_to_big(_epoch);
    std::vector<uint8_t> epochBytes(reinterpret_cast<uint8_t*>(&epochBE),
                                reinterpret_cast<uint8_t*>(&epochBE) + sizeof(epochBE));

    RLPStream list;
    list << epochBytes << *_encryptedKeyPlusData;

    serializedData = make_shared<vector<uint8_t> >(list.encode());
}

BiteCiphertext::BiteCiphertext(const std::shared_ptr<std::vector<uint8_t> > &_data, epoch_id _currentEpochId) {
    CHECK_STATE(_data);

    // parse RLP-encoded tx data field
    // RLP structure: [epochId1, encryptedBITEData]
    // encryptedBITEData may optionally have 1 or 2 encrypted AES keys assosiated with it
    RLPItem rlp(*_data);
    CHECK_STATE2(rlp.isList(), "RLP item is not a list");
    CHECK_STATE2(rlp.size() == 2, "RLP item should have exactly 2 items");

    // read encryptedBITEData
    auto encryptedBITEDataBytes = make_shared<std::vector<uint8_t>>(rlp[1].asBytes());
    CHECK_STATE2( encryptedBITEDataBytes->size() > BITE_CIPHERTEXT_MIN_LEN,
                  "Incorrectly formatted BITE transaction: Encrypted data size is not at least " +
                  to_string(BITE_CIPHERTEXT_MIN_LEN) + " bytes, found: " +
                  to_string(encryptedBITEDataBytes->size()));

    // parse epochId
    auto epochIdBytes = rlp[0].asBytes();
    CHECK_STATE2(epochIdBytes.size() <= sizeof(uint64_t), "Epoch id too long");
    uint64_t epochIdCandidate = u256( epochIdBytes ).convert_to< uint64_t >();
    // first byte stands for the number of encrypted AES keys in payload
    uint8_t numEncryptedAESKeys = encryptedBITEDataBytes->at(0);
    // if 2 encrypted AES keys are submitted
    if ( numEncryptedAESKeys == 1 ) {
        // set epochId
        epoch = epochIdCandidate;
        // set encrypted data and AES key
        keyPlusEncryptedData = encryptedBITEDataBytes;
        auto keyVec = std::array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN>();
        // first byte stands for the number of keys in payload - skip it when parsing manually
        std::copy_n(keyPlusEncryptedData->begin() + KEY_COUNT_BYTE_OFFSET,
                    BITE_ENCRYPTED_AES_KEY_LEN, keyVec.begin());
        encryptedAESKey = EncryptedAESKey(keyVec);
    } else {
        // if encryptedBITEData contains AES key encrypted with 2 BLS keys
        // need to determine which one was used to encrypt the original message based on epochId
        // AES key encrypted with wrong BLS key will not be added to keyPlusEncryptedData
        // do not validate inputs
        bool toValidate = false;
        libBLS::Ciphertext encryptedBITEData = libBLS::Ciphertext::fromBytes(
                    *encryptedBITEDataBytes, toValidate );
        size_t keyIndexToKeep;
        if ( epochIdCandidate != _currentEpochId  ) {
            // set epochId
            epoch = epochIdCandidate + 1;
            // set target encrypted AES key
            keyIndexToKeep = 1;
        } else {
            // set epochId
            epoch = epochIdCandidate;
            // set target encrypted AES key
            keyIndexToKeep = 0;
        }
        // set encrypted data and AES key
        encryptedBITEData.keepKey( keyIndexToKeep );
        keyPlusEncryptedData = make_shared<vector<uint8_t>>( encryptedBITEData.toBytes() );
        encryptedAESKey.emplace(EncryptedAESKey(encryptedBITEData.getTargetKey().toBytes()));
    }
    
    CHECK_STATE2((uint64_t) _currentEpochId == epoch, "Incorrectly formatted BITE transaction: wrong epochId");

    CHECK_STATE2(keyPlusEncryptedData->size() >= BITE_ENCRYPTED_AES_KEY_LEN,
            "Incorrectly formatted BITE transaction: Encrypted data size is not at least " +
                 to_string(BITE_ENCRYPTED_AES_KEY_LEN) + " bytes, found: " +
                 to_string(keyPlusEncryptedData->size()));

    serializedData = _data;
}

EncryptedAESKey &BiteCiphertext::getEncryptedAESKey() {
    return encryptedAESKey.value();
}
const shared_ptr< EncryptedData >& BiteCiphertext::getKeyPlusEncryptedData() const {
    return keyPlusEncryptedData;
}

uint64_t BiteCiphertext::getEpoch() {
    return epoch;
}

ptr<vector<uint8_t> > &BiteCiphertext::getSerializedData() {
    return serializedData;
}
