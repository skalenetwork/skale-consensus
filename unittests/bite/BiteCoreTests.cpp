#include "thirdparty/catch.hpp"

#ifdef BITE

#include "bite/BiteCore.h"
#include "libBLS/threshold_encryption/threshold_encryption.h"
#include "libBLS/threshold_encryption/ThresholdEncryption.h"
#include "libBLS/threshold_encryption/TEPublicKey.h"
#include "libBLS/test/utils.h"

CATCH_TEST_CASE("BiteCore mock encryption roundtrips data", "[bite][core]") {
    BiteCore core;
    core.useMockCrypto();

    std::vector<uint8_t> message{0x10, 0x20, 0x30};
    auto ciphertext = core.encryptData(libBLS::TEPublicKey::random(), message);
    CATCH_REQUIRE_FALSE(ciphertext.empty());

    libBLS::AES256Key aesKey{};
    auto decrypted = core.decryptData(aesKey, ciphertext);
    CATCH_REQUIRE(decrypted == message);
}


CATCH_TEST_CASE("BiteCore real encryption produces threshold ciphertext bytes", "[bite][core]") {
    BiteCore core;
    std::vector<uint8_t> message{0x01, 0x02, 0x03, 0x04};

    auto keys = generateKeys(1, 1);

    auto ciphertext = core.encryptData(keys.commonPublic, message);
    CATCH_REQUIRE(ciphertext.size() > message.size());
    CATCH_REQUIRE(ciphertext[0] == 1);  // number of encrypted keys

    libBLS::Ciphertext teCiphertext =
        libBLS::Ciphertext::fromBytes(ciphertext, true /* validate */);
    CATCH_REQUIRE(teCiphertext.keys.size() == 1);

    auto share = libBLS::ThresholdEncryption::partialDecrypt(
        teCiphertext.keys[0], keys.secretKeys[0]);
    
    libBLS::TEDecryptSet decryptSet(1, 1);
    decryptSet.addDecryptShare(share);

    auto decryptedKey = libBLS::ThresholdEncryption::combineShares(
        teCiphertext.keys[0], decryptSet);

    auto decrypted = core.decryptData(decryptedKey, ciphertext, true /* validate */);
    CATCH_REQUIRE(decrypted == message);
}


CATCH_TEST_CASE("BiteCore validateCiphertexts returns public values when all valid", "[bite][core]") {
    BiteCore core;

    // single valid ciphertext
    auto key = libBLS::ThresholdEncryption::encrypt(
        std::vector<uint8_t>{0x01, 0x02, 0x03}, libBLS::TEPublicKey::random()).keys[0];
    auto result = core.validateCiphertexts({key});
    CATCH_REQUIRE(result.allValid);
    CATCH_REQUIRE(result.validationResults.size() == 1);
    CATCH_REQUIRE(result.validationResults[0]);
    CATCH_REQUIRE(result.publicDecryptionValues.size() == 1);

    // multiple valid ciphertexts
    std::vector<libBLS::CipheredKey> keys;
    const size_t numKeys = 20;
    for (size_t i = 0; i < numKeys; i++) {
        keys.push_back(libBLS::ThresholdEncryption::encrypt(
            std::vector<uint8_t>{uint8_t(i)}, libBLS::TEPublicKey::random()).keys[0]);
    }

    result = core.validateCiphertexts(keys);
    CATCH_REQUIRE(result.allValid);
    CATCH_REQUIRE(result.validationResults.size() == numKeys);
    for (const auto& v : result.validationResults) {
        CATCH_REQUIRE(v);
    }
    CATCH_REQUIRE(result.publicDecryptionValues.size() == numKeys);
}


CATCH_TEST_CASE("BiteCore validateCiphertexts flags invalid ciphertexts", "[bite][core]") {
    BiteCore core;

    auto validKey = libBLS::ThresholdEncryption::encrypt(
        std::vector<uint8_t>{0x0A, 0x0B, 0x0C}, libBLS::TEPublicKey::random()).keys[0];
    auto invalidKey = libBLS::CipheredKey(
        validKey.getU(), validKey.getV(), libBLS::algebra::G1Point::random(), false);

    auto result = core.validateCiphertexts({validKey, invalidKey});
    CATCH_REQUIRE_FALSE(result.allValid);
    CATCH_REQUIRE(result.validationResults.size() == 2);
    CATCH_REQUIRE(result.validationResults[0]);
    CATCH_REQUIRE_FALSE(result.validationResults[1]);
    CATCH_REQUIRE(result.publicDecryptionValues.empty());

    // several random  faulty items
    std::vector<libBLS::CipheredKey> keys;
    std::vector<bool> expectedValidity;
    const size_t numKeys = 30;
    for (size_t i = 0; i < numKeys; i++) {
        expectedValidity.push_back(rand()  % 5 != 0);  // ~20% invalid
        if (expectedValidity.back()) { // valid key
            auto validKey = libBLS::ThresholdEncryption::encrypt(
                std::vector<uint8_t>{0x01, 0x02, 0x03}, libBLS::TEPublicKey::random()).keys[0];
            keys.push_back(validKey);
        } else {
            keys.push_back(libBLS::CipheredKey::random());
        }
    }

    result = core.validateCiphertexts(keys);
    CATCH_REQUIRE_FALSE(result.allValid);
    CATCH_REQUIRE(result.validationResults.size() == numKeys);
    for (size_t i = 0; i < numKeys; i++) {
        if (!expectedValidity[i]) {
            CATCH_REQUIRE_FALSE(result.validationResults[i]);
        } else {
            CATCH_REQUIRE(result.validationResults[i]);
        }
    }
    CATCH_REQUIRE(result.publicDecryptionValues.empty());
}

#endif
