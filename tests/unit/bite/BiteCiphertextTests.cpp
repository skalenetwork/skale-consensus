#include "thirdparty/catch.hpp"

#include "bite/BiteCiphertext.h"
#include "bite/BiteCodec.h"
#include "bite/Constants.h"
#include "libBLS/threshold_encryption/threshold_encryption.h"
#include "rlp/RLPStream.h"
#include <boost/endian/conversion.hpp>

#include <vector>

namespace {

// helper to build a minimal valid key+ciphertext buffer with one AES key
std::vector<uint8_t> makeKeyPlusEncryptedData(uint8_t countByte = 1) {
    std::vector<uint8_t> data;
    data.reserve(BITE_ENCRYPTED_AES_KEY_LEN + 1);
    data.push_back(countByte);
    for (size_t i = 0; i < BITE_ENCRYPTED_AES_KEY_LEN; ++i) {
        data.push_back(static_cast<uint8_t>(i & 0xFF));
    }
    return data;
}

} // namespace

CATCH_TEST_CASE("BiteCiphertext parses single-key epoched payload", "[bite][ciphertext][success]") {
    const uint64_t epochId = 7;
    auto keyPlusEncryptedData = makeKeyPlusEncryptedData();
    auto serialized = BiteCodec::encodeEpochedBiteData(keyPlusEncryptedData, epochId);

    auto serializedPtr = std::make_shared<std::vector<uint8_t>>(serialized);
    BiteCiphertext ct(serializedPtr, epochId);

    CATCH_REQUIRE(ct.getEpoch() == epochId);
    CATCH_REQUIRE(ct.getKeyPlusEncryptedData());
    CATCH_REQUIRE(*ct.getKeyPlusEncryptedData() == keyPlusEncryptedData);

    auto keyArr = ct.getEncryptedAESKey().data();
    std::vector<uint8_t> parsedKey(keyArr.begin(), keyArr.end());
    std::vector<uint8_t> expectedKey(keyPlusEncryptedData.begin() + 1, keyPlusEncryptedData.begin() + 1 + BITE_ENCRYPTED_AES_KEY_LEN);
    CATCH_REQUIRE(parsedKey == expectedKey);
}

CATCH_TEST_CASE("BiteCiphertext chooses correct key in dual-key payload", "[bite][ciphertext][dual-key]") {
    std::vector<libBLS::TEPublicKey> pubKeys = { libBLS::TEPublicKey::random(), libBLS::TEPublicKey::random() };
    auto cipher = libBLS::ThresholdEncryption::encrypt(std::vector<uint8_t>(16, 0x11), pubKeys);

    auto baseCipher = cipher.toBytes();

    const uint64_t epochId = 5;
    auto serialized = BiteCodec::encodeEpochedBiteData(baseCipher, epochId);
    auto serializedPtr = std::make_shared<std::vector<uint8_t>>(serialized);

    // Expected key bytes from original ciphertext
    auto key0Bytes = cipher.getKeys().at(0).toBytes();
    auto key1Bytes = cipher.getKeys().at(1).toBytes();

    // Matching epoch keeps key0
    BiteCiphertext ctMatch(serializedPtr, epochId);
    auto keyMatchArr = ctMatch.getEncryptedAESKey().data();
    std::vector<uint8_t> parsedKeyMatch(keyMatchArr.begin(), keyMatchArr.end());
    CATCH_REQUIRE(parsedKeyMatch == std::vector<uint8_t>(key0Bytes.begin(), key0Bytes.end()));

    // Mismatched epoch picks the "next" key (index 1)
    BiteCiphertext ctOther(serializedPtr, epochId + 1);
    auto keyOtherArr = ctOther.getEncryptedAESKey().data();
    std::vector<uint8_t> parsedKeyOther(keyOtherArr.begin(), keyOtherArr.end());
    CATCH_REQUIRE(parsedKeyOther == std::vector<uint8_t>(key1Bytes.begin(), key1Bytes.end()));
}


CATCH_TEST_CASE("BiteCiphertext rejects malformed payloads", "[bite][ciphertext][failure]") {
    const uint64_t epochId = 3;

    CATCH_SECTION("non-RLP data throws") {
        auto badData = std::make_shared<std::vector<uint8_t>>(std::initializer_list<uint8_t>{0x01, 0x02});
        CATCH_REQUIRE_THROWS(BiteCiphertext(badData, epochId));
    }

    CATCH_SECTION("ciphertext too short throws") {
        std::vector<uint8_t> tooSmallKey(BITE_ENCRYPTED_AES_KEY_LEN, 0xAA); // size == BITE_ENCRYPTED_AES_KEY_LEN -> invalid

        // encode manually - calling BiteCodec::encodeEpochedBiteData will CHECK_STATE on size
        uint64_t epochBE = boost::endian::native_to_big(epochId);
        std::vector<uint8_t> epochBytes(reinterpret_cast<uint8_t*>(&epochBE),
                                    reinterpret_cast<uint8_t*>(&epochBE) + sizeof(epochBE));

        RLPStream list;
        list << epochBytes << tooSmallKey;
        auto serialized = list.encode();

        // check for exception
        auto serializedPtr = std::make_shared<std::vector<uint8_t>>(serialized);
        CATCH_REQUIRE_THROWS(BiteCiphertext(serializedPtr, epochId));
    }

    CATCH_SECTION("mismatched epoch throws") {
        auto keyPlusEncryptedData = makeKeyPlusEncryptedData();
        auto serialized = BiteCodec::encodeEpochedBiteData(keyPlusEncryptedData, epochId);
        auto serializedPtr = std::make_shared<std::vector<uint8_t>>(serialized);
        CATCH_REQUIRE_THROWS(BiteCiphertext(serializedPtr, epochId + 1));
    }
}
