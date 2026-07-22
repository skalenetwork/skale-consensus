#include "bite/BiteCodec.h"
#include "bite/Constants.h"
#include "rlp/RLP.h"
#include "rlp/RLPStream.h"
#include "Log.h"

#include <boost/endian/conversion.hpp>
#include <boost/endian/arithmetic.hpp>

// ==================== BiteCiphertext parsing  from Transaction fields ==================== //

ptr<BiteCiphertext> BiteCodec::tryParseEncryptedRegularTxFields(
        std::vector<uint8_t>& _to, ptr<std::vector<uint8_t>> _data, epoch_id _currentEpochId) {
    // compare _to field to BITE magic number
    if (_to.size() != ADDRESS_SIZE ||
        !std::equal(BITE_ADDRESS_AS_BYTE_ARRAY,
                    BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE,
                    _to.begin())) {
        return nullptr;
    }

    return std::make_shared<BiteCiphertext>(_data,  _currentEpochId);
}

std::shared_ptr<std::vector<std::shared_ptr<BiteCiphertext>>> BiteCodec::tryParseEncryptedCTXArgs(
        const std::vector<uint8_t>& _dataField, epoch_id _currentEpochId) {
    // compare first 4 bytes to BITE2 expected function selector
    if (_dataField.size() < BITE2_FUNCTION_SELECTOR_SIZE_BYTES ||
        std::memcmp(
            _dataField.data(),
            BITE2_FUNCTION_SELECTOR_AS_BYTE_ARRAY,
            BITE2_FUNCTION_SELECTOR_SIZE_BYTES
        ) != 0
    ) {
        return nullptr;
    }

    // It claims to be CTX, so it must contain an RLP payload.
    CHECK_STATE2(
        _dataField.size() > BITE2_FUNCTION_SELECTOR_SIZE_BYTES,
        "CTX data is missing its RLP payload"
    );

    // Strip the selector and parse the complete CTX RLP envelope.
    auto dataWithoutSelector = std::vector<uint8_t>(
        _dataField.begin() + BITE2_FUNCTION_SELECTOR_SIZE_BYTES,
        _dataField.end()
    );

    size_t rlpOffset = 0;
    RLPItem rlpItem(dataWithoutSelector, rlpOffset);

    CHECK_STATE2(
        rlpOffset == dataWithoutSelector.size(),
        "CTX RLP payload contains trailing bytes"
    );

    CHECK_STATE2(rlpItem.isList(), "CTX payload must be an RLP list");
    CHECK_STATE2(rlpItem.size() == 2, "CTX payload must contain encrypted and plaintext lists");

    const RLPItem& encryptedArgsRLP = rlpItem[0];
    const RLPItem& plaintextArgsRLP = rlpItem[1];

    CHECK_STATE2(encryptedArgsRLP.isList(), "CTX encrypted arguments must be an RLP list");
    CHECK_STATE2(plaintextArgsRLP.isList(), "CTX plaintext arguments must be an RLP list");

    auto encryptedCTXArgs =
        std::make_shared<std::vector<std::shared_ptr<BiteCiphertext>>>();
    encryptedCTXArgs->reserve(encryptedArgsRLP.size());

    for (size_t i = 0; i < encryptedArgsRLP.size(); ++i) {
        const RLPItem& encryptedArg = encryptedArgsRLP[i];

        CHECK_STATE2(!encryptedArg.isList(), "CTX encrypted argument must be an RLP byte string");

        auto argData = std::make_shared<std::vector<uint8_t>>(encryptedArg.asBytes());
        encryptedCTXArgs->emplace_back(
            std::make_shared<BiteCiphertext>(argData, _currentEpochId)
        );
    }

    return encryptedCTXArgs;
}

// ==================== BiteCiphertext building for Transaction fields ==================== //


// Encode CTX arguments given serialized encrypted args + plaintext args
std::vector<uint8_t> BiteCodec::encodeCTXData(
    const std::vector<std::vector<uint8_t>>& encryptedSerializedArgs,
    const std::vector<std::vector<uint8_t>>& plainArgs
) {
    RLPStream encryptedRlp;
    for (const auto& enc : encryptedSerializedArgs) {
        encryptedRlp << enc;
    }

    RLPStream plainRlp;
    for (const auto& plain : plainArgs) {
        plainRlp << plain;
    }

    RLPStream allArgs;
    allArgs << encryptedRlp << plainRlp;

    auto finalData = allArgs.encode();

    std::vector<uint8_t> data(BITE2_FUNCTION_SELECTOR_SIZE_BYTES + finalData.size());

    // prefix with function selector
    std::memcpy(data.data(), BITE2_FUNCTION_SELECTOR_AS_BYTE_ARRAY, BITE2_FUNCTION_SELECTOR_SIZE_BYTES);

    // append RLP data
    std::memcpy(data.data() + BITE2_FUNCTION_SELECTOR_SIZE_BYTES, finalData.data(), finalData.size());

    return data;
}



std::vector<uint8_t> BiteCodec::encodeRegularTxPayload(
        const std::vector<uint8_t>& _plainData, const std::vector<uint8_t>& _to) {
    RLPStream stream;
    stream << _plainData << _to;
    return stream.encode();
}


vector<uint8_t> BiteCodec::decryptCiphertext(const BiteCiphertext& _bite, const libBLS::AES256Key& _decryptedAESKey, const BiteCore& core) {
    auto encryptedData = _bite.getKeyPlusEncryptedData();
    CHECK_STATE(encryptedData);
    return core.decryptData(_decryptedAESKey, *encryptedData);
}



// ======================== Parsing decrypted data into tx fields ======================== //

DecryptedRegularTxFields BiteCodec::parseRegularTxDecryptedData(
        const vector<uint8_t> &_data) {

    RLPItem decryptedDataRlp(_data);
    CHECK_STATE2(decryptedDataRlp.isList(), "Encrypted data rlp size must be a list");
    CHECK_STATE2(decryptedDataRlp.size() == 2,
                 "Encrypted data rlp list must have exactly 2 elements");
    // extract decrypted data and to fields
    vector<uint8_t> dataField = decryptedDataRlp[0].asBytes();
    vector<uint8_t> toBytes = decryptedDataRlp[1].asBytes();

    CHECK_STATE2(toBytes.size() == ADDRESS_SIZE,
                 "Decrypted tx.to field must be exactly 20 bytes");
    
    std::array<uint8_t, 20> toField;
    std::copy(toBytes.begin(), toBytes.end(), toField.begin());

    auto decryptedFields = DecryptedRegularTxFields {
            .data = std::move(dataField),
            .to = std::move(toField),
    };

    return decryptedFields;
}


std::vector<uint8_t> BiteCodec::encodeEpochedBiteData(
    const std::vector<uint8_t>& _keyPlusEncryptedData, uint64_t _epoch) {
    CHECK_STATE(_keyPlusEncryptedData.size() > BITE_ENCRYPTED_AES_KEY_LEN);

    // build serialized RLP-encoded data field
    uint64_t epochBE = boost::endian::native_to_big(_epoch);
    std::vector<uint8_t> epochBytes(reinterpret_cast<uint8_t*>(&epochBE),
                                reinterpret_cast<uint8_t*>(&epochBE) + sizeof(epochBE));

    RLPStream list;
    list << epochBytes << _keyPlusEncryptedData;

    return list.encode();
}

BiteCodec::EpochedBiteData BiteCodec::decodeEpochedBiteData(
    const std::vector<uint8_t>& _dataField) {
    RLPItem rlpItem(_dataField);
    CHECK_STATE(rlpItem.isList());
    CHECK_STATE(rlpItem.size() == 2); // [epochId, keyPlusEncryptedData]

    // parse epochId
    auto epochIdBytes = rlpItem[0].asBytes();
    CHECK_STATE2(epochIdBytes.size() <= sizeof(uint64_t), "Epoch id too long");
    uint64_t epochId = u256( epochIdBytes ).convert_to< uint64_t >();

    auto keyPlusEncryptedData = std::make_shared<std::vector<uint8_t>>(rlpItem[1].asBytes());

    return EpochedBiteData {
        .epochId = epochId,
        .keyPlusEncryptedData = keyPlusEncryptedData
    };
}


// Helper function to split string_view by commas
std::vector<std::string_view> BiteCodec::splitShares(std::string_view s) {
    std::vector<std::string_view> out;

    while (!s.empty()) {
        size_t pos = s.find(',');
        if (pos == std::string_view::npos) {
            out.push_back(s);
            break;
        }
        out.push_back(s.substr(0, pos));
        s.remove_prefix(pos + 1);
    }

    return out;
}
