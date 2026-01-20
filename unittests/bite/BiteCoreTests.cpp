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


CATCH_TEST_CASE("BiteCore validateCiphertexts with AAD validates correctly", "[bite][core][aad]") {
    BiteCore core;
    
    // Create a ciphertext with AAD using core.encryptData
    std::vector<uint8_t> message{0x01, 0x02, 0x03};
    std::vector<uint8_t> aad{0xAA, 0xBB, 0xCC, 0xDD};  // SC address as AAD
    
    auto keys = generateKeys(1, 1);
    auto ciphertextBytes = core.encryptData(keys.commonPublic, message, aad);
    auto ciphertext = libBLS::Ciphertext::fromBytes(ciphertextBytes, false);
    
    // Validate with correct AAD
    std::vector<std::vector<uint8_t>> aadVec{aad};
    auto result = core.validateCiphertexts(ciphertext.keys, &aadVec);
    CATCH_REQUIRE(result.allValid);
    CATCH_REQUIRE(result.validationResults[0]);
    CATCH_REQUIRE(result.publicDecryptionValues.size() == 1);
    
    // Validate with wrong AAD should fail
    std::vector<uint8_t> wrongAad{0xFF, 0xFF, 0xFF, 0xFF};
    std::vector<std::vector<uint8_t>> wrongAadVec{wrongAad};
    result = core.validateCiphertexts(ciphertext.keys, &wrongAadVec);
    CATCH_REQUIRE_FALSE(result.allValid);
    CATCH_REQUIRE_FALSE(result.validationResults[0]);
    
    // Validate without AAD should also fail (encrypted with AAD, validated without)
    result = core.validateCiphertexts(ciphertext.keys, nullptr);
    CATCH_REQUIRE_FALSE(result.allValid);
}


CATCH_TEST_CASE("BiteCore validateCiphertexts with partial AAD", "[bite][core][aad][partial]") {
    BiteCore core;
    
    // Create ciphertexts: first 2 with AAD, last 2 without
    std::vector<uint8_t> message{0x01, 0x02};
    std::vector<uint8_t> aad1{0x11, 0x11, 0x11};
    std::vector<uint8_t> aad2{0x22, 0x22, 0x22};
    
    libBLS::EncryptMetaData meta1;
    meta1.associatedDataTE = aad1;
    auto cipher1 = libBLS::ThresholdEncryption::encrypt(
        message, libBLS::TEPublicKey::random(), meta1).keys[0];
    
    libBLS::EncryptMetaData meta2;
    meta2.associatedDataTE = aad2;
    auto cipher2 = libBLS::ThresholdEncryption::encrypt(
        message, libBLS::TEPublicKey::random(), meta2).keys[0];
    
    // Ciphertexts without AAD
    auto cipher3 = libBLS::ThresholdEncryption::encrypt(
        message, libBLS::TEPublicKey::random()).keys[0];
    auto cipher4 = libBLS::ThresholdEncryption::encrypt(
        message, libBLS::TEPublicKey::random()).keys[0];
    
    std::vector<libBLS::CipheredKey> allCiphers{cipher1, cipher2, cipher3, cipher4};
    
    // Partial AAD: only first 2 entries
    std::vector<std::vector<uint8_t>> partialAad{aad1, aad2};
    
    auto result = core.validateCiphertexts(allCiphers, &partialAad);
    CATCH_REQUIRE(result.allValid);
    CATCH_REQUIRE(result.validationResults.size() == 4);
    for (size_t i = 0; i < 4; i++) {
        CATCH_REQUIRE(result.validationResults[i]);
    }
    CATCH_REQUIRE(result.publicDecryptionValues.size() == 4);
}


CATCH_TEST_CASE("BiteCore encryptData with AAD produces different ciphertext", "[bite][core][aad]") {
    BiteCore core;
    
    std::vector<uint8_t> message{0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> aad{0xDE, 0xAD, 0xBE, 0xEF};
    
    auto keys = generateKeys(1, 1);
    
    // Encrypt without AAD
    auto ciphertextNoAad = core.encryptData(keys.commonPublic, message);
    
    // Encrypt with AAD
    auto ciphertextWithAad = core.encryptData(keys.commonPublic, message, aad);
    
    // Both should be non-empty and have same structure
    CATCH_REQUIRE_FALSE(ciphertextNoAad.empty());
    CATCH_REQUIRE_FALSE(ciphertextWithAad.empty());
    
    // Parse and validate - ciphertext with AAD should only validate with AAD
    auto teNoAad = libBLS::Ciphertext::fromBytes(ciphertextNoAad, false);
    auto teWithAad = libBLS::Ciphertext::fromBytes(ciphertextWithAad, false);
    
    // Validate without AAD - should pass for first, fail for second
    std::vector<libBLS::CipheredKey> keysVec{teNoAad.keys[0]};
    auto result = core.validateCiphertexts(keysVec, nullptr);
    CATCH_REQUIRE(result.allValid);
    
    std::vector<libBLS::CipheredKey> keysVec2{teWithAad.keys[0]};
    result = core.validateCiphertexts(keysVec2, nullptr);
    CATCH_REQUIRE_FALSE(result.allValid);
    
    // Validate with AAD - should pass for second
    std::vector<std::vector<uint8_t>> aadVec{aad};
    result = core.validateCiphertexts(keysVec2, &aadVec);
    CATCH_REQUIRE(result.allValid);
}

#endif
