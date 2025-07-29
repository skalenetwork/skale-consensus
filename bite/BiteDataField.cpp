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
    // RLP structure: [epochId1, epochId2, encryptedBITEData]
    // where epochId2 is an optional element
    RLPItem rlp(*_data);
    CHECK_STATE2(rlp.isList(), "RLP item is not a list");
    CHECK_STATE2(rlp.size() > 1, "RLP item should have at least 2 item");
    CHECK_STATE2(rlp.size() < 4, "RLP item should not have more than 3 items");
    
    const uint64_t currentEpoch = _currentEpochId.convert_to<uint64_t>();

    // encrypted data always goes last
    auto encryptedBITEDataBytes = make_shared<std::vector<uint8_t>>(rlp[rlp.size() - 1].asBytes());
    CHECK_STATE2( encryptedBITEDataBytes->size() > BITE_CIPHERTEXT_MIN_LEN,
                  "Incorrectly formatted BITE transaction: Encrypted data size is not at least " +
                  to_string(BITE_CIPHERTEXT_MIN_LEN) + " bytes, found: " +
                  to_string(encryptedBITEDataBytes->size()));

    // parse epochId
    auto epochIdBytes = rlp[0].asBytes();
    CHECK_STATE2(epochIdBytes.size() <= sizeof(uint64_t), "Epoch id too long");
    uint64_t epochIdCandidate = u256( epochIdBytes ).convert_to< uint64_t >();
    if ( rlp.size() == 2 ) {
        // set epochId
        epoch = epochIdCandidate;
        keyPlusEncryptedData = std::move( encryptedBITEDataBytes );
        auto keyVec = std::make_shared<std::array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN> >();
        std::copy_n(keyPlusEncryptedData->begin(), BITE_ENCRYPTED_AES_KEY_LEN, keyVec->begin());
        encryptedAESKey = make_shared<EncryptedAESKey>(keyVec);
    } else {
        // if encryptedBITEData contains AES key encrypted with 2 BLS keys
        // need to determine which one was used based on epochId
        // AES key encrypted with wrong BLS key will not be added to keyPlusEncryptedData
        libBLS::Ciphertext encryptedBITEData = libBLS::Ciphertext::fromBytes( *encryptedBITEDataBytes );
        if ( epochIdCandidate != currentEpoch  ) {
            // set epochId
            epochIdBytes = rlp[1].asBytes();
            CHECK_STATE2(epochIdBytes.size() <= sizeof(uint64_t), "Epoch id too long");
            epoch = u256( epochIdBytes ).convert_to< uint64_t >();
            // set target encrypted AES key
            encryptedBITEData.keepKey( 1 );
        } else {
            // set target encrypted AES key
            encryptedBITEData.keepKey( 0 );
        }
        // set encrypted data and AES key
        keyPlusEncryptedData = make_shared<vector<uint8_t>>( encryptedBITEData.toBytes() );
        encryptedAESKey = make_shared<EncryptedAESKey>(
                    make_shared<std::array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN>>(
                        encryptedBITEData.getTargetKey().toBytes()));
    }
    
    CHECK_STATE2(currentEpoch == epoch, "Incorrectly formatted BITE transaction: wrong epochId");
    CHECK_STATE2(keyPlusEncryptedData->size() >= BITE_ENCRYPTED_AES_KEY_LEN,
            "Incorrectly formatted BITE transaction: Encrypted data size is not at least " +
                 to_string(BITE_ENCRYPTED_AES_KEY_LEN) + " bytes, found: " +
                 to_string(keyPlusEncryptedData->size()));

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
