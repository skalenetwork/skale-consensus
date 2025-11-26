#include "bite/BiteCodec.h"
#include "rlp/RLP.h"
#include "rlp/RLPStream.h"
#include "Log.h"

// ==================== BiteCiphertext parsing  from Transaction fields ==================== //

std::shared_ptr<BiteCiphertext> BiteCodec::tryParseEncryptedRegularTxFields(
        std::vector<uint8_t>& _to, std::vector<uint8_t>& _data, epoch_id _currentEpochId) {
    // compare _to field to BITE magic number
    if (!std::equal(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE,
                    _to.begin())) {
        return nullptr;
    }

    return std::make_shared<BiteCiphertext>(_data,  _currentEpochId);
}

std::shared_ptr<std::vector<std::shared_ptr<BiteCiphertext>>> BiteCodec::tryParseEncryptedCATArgs(
        const std::vector<uint8_t>& _dataField, epoch_id _currentEpochId) {
    // compare first 4 bytes to BITE2 expected function selector
    if (_dataField.size() < BITE_FUNCTION_SELECTOR_SIZE_BYTES ||
        std::memcmp(
            _dataField.data(),
            BITE_FUNCTION_SELECTOR_AS_BYTE_ARRAY,
            BITE_FUNCTION_SELECTOR_SIZE_BYTES
        ) != 0
    ) {
        return nullptr;
    }

    // Parse args as RLP list
    // Data comes as:
    // [funcSelector, RLP( RLP(cipher1, cipher2, ...), RLP(plaintext1, plaintext2, ...) )]
    // offset function selector
    auto dataWithoutSelector = std::vector<uint8_t>(_dataField.begin() + BITE_FUNCTION_SELECTOR_SIZE_BYTES, _dataField.end());
    RLPItem rlpItem(dataWithoutSelector);
    CHECK_STATE(rlpItem.isList());
    CHECK_STATE(rlpItem.size() == 2); // RLP(ciphertexts, plaintexts)
    RLPItem encryptedArgsRLP = rlpItem[0];
    CHECK_STATE(encryptedArgsRLP.isList());

    auto encryptedCATArgs = std::make_shared<std::vector<std::shared_ptr<BiteCiphertext>>>();

    encryptedCATArgs->reserve(encryptedArgsRLP.size());
    for (size_t i = 0; i < encryptedArgsRLP.size(); i++) {
        auto argData = std::make_shared<std::vector<uint8_t>>(encryptedArgsRLP[i].asBytes());
        BiteCiphertext biteCiphertext(argData, _currentEpochId);
        encryptedCATArgs->emplace_back( std::make_shared<BiteCiphertext>(biteCiphertext) );
    }
    return encryptedCATArgs;
}





// ==================== BiteCiphertext building for Transaction fields ==================== //


vector<uint8_t> BiteCodec::buildCATData(const libBLS::TEPublicKey& _key, size_t numberOfCiphertexts, const BiteCore& core) {
    RLPStream allArgs;
    
    RLPStream encryptedArgs;
    for (size_t i = 0; i < numberOfCiphertexts; ++i) {
        std::vector<uint8_t> rndData;
        std::vector<uint8_t> encryptedData;
        rndData.resize(numberOfCiphertexts * 10);
        encryptedData = core.encryptData(_key, rndData);
        BiteCiphertext biteCiphertext(
            make_shared<vector<uint8_t>>(std::move(encryptedData)),
            0 // epoch id not relevant here
        );
        encryptedArgs << *biteCiphertext.getSerializedData();
    }
    RLPStream plainArgs;
    size_t numPlaintexts = numberOfCiphertexts - 1;
    for (size_t i = 0; i < numPlaintexts; ++i) {
        std::vector<uint8_t> rndData;
        rndData.resize(numberOfCiphertexts * 5);
        plainArgs << rndData;
    }

    allArgs << encryptedArgs << plainArgs;
    auto finalData = allArgs.encode();

    std::vector<uint8_t> data;
    data.reserve(BITE_FUNCTION_SELECTOR_SIZE_BYTES + finalData.size());

    // prefix with function selector 
    data.insert(
        data.end(),
        BITE_FUNCTION_SELECTOR_AS_BYTE_ARRAY,
        BITE_FUNCTION_SELECTOR_AS_BYTE_ARRAY + BITE_FUNCTION_SELECTOR_SIZE_BYTES
    );

    // append RLP data
    data.insert(data.end(), finalData.begin(), finalData.end());

    return std::move(data);
}


std::vector<uint8_t> BiteCodec::buildRegularTxData(const libBLS::TEPublicKey& _key, 
        const std::vector<uint8_t>& _plainData, const std::vector<uint8_t>& _to, const BiteCore& core) {
    // RLP encode
    RLPStream stream;
    stream << _plainData << _to;
    return core.encryptData(_key, stream.encode());
}


vector<uint8_t> BiteCodec::decryptCiphertext(const BiteCiphertext& _bite, const libBLS::AES256Key& _decryptedAESKey, const BiteCore& core) {
    auto encryptedData = _bite.getKeyPlusEncryptedData();
    CHECK_STATE(encryptedData);
    return core.decryptData(_decryptedAESKey, *encryptedData);
}



// ======================== Parsing decrypted data into tx fields ======================== //

DecryptedRegularTxFields BiteCodec::parseRegularTxDecryptedData(
        const vector<uint8_t> &_data) {

    CHECK_STATE2(_data.size() >= ADDRESS_SIZE,
                 "Decrypted data is not long enough to include the original tx.to field!");

    RLPItem decryptedDataRlp(_data);
    CHECK_STATE2(decryptedDataRlp.isList(), "Encrypted data rlp size must be a list");
    CHECK_STATE2(decryptedDataRlp.size() == 2,
                 "Encrypted data rlp lsit must have exactly 2 elements");
    // extract decrypted data and to fields
    vector<uint8_t> dataField = decryptedDataRlp[0].asBytes();
    
    std::array<uint8_t, 20> toField;
    std::copy(decryptedDataRlp[1].asBytes().begin(), decryptedDataRlp[1].asBytes().end(), toField.begin());

    auto decryptedFields = DecryptedRegularTxFields {
            .data = std::move(decryptedDataRlp[0].asBytes()),
            .to = std::move(toField),
    };

    return decryptedFields;
}