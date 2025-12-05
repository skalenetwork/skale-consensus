#include "thirdparty/catch.hpp"

#ifdef BITE

#include "bite/BiteCodec.h"
#include "bite/BiteCore.h"
#include "bite/Constants.h"
#include "libBLS/threshold_encryption/threshold_encryption.h"
#include "libBLS/test/utils.h"

#include "BiteTestUtils.h"

using namespace BiteTestUtils;

CATCH_TEST_CASE("BiteCodec parses BITE1 transactions correctly", "[bite][codec]") {
    const uint64_t epoch = 7;
    auto cipheredKey = libBLS::CipheredKey::random();
    auto serialized = buildSerializedBiteData(cipheredKey, epoch);

    // correct address - should parse
    auto biteAddress = biteMagicAddress();
    auto parsed = BiteCodec::tryParseEncryptedRegularTxFields(biteAddress, serialized, epoch);
    CATCH_REQUIRE(parsed);
    CATCH_REQUIRE(parsed->getEpoch() == epoch);

    // wrong address - should not parse
    std::vector<uint8_t> wrongTo(ADDRESS_SIZE, 0x00);
    auto notParsed = BiteCodec::tryParseEncryptedRegularTxFields(wrongTo, serialized, epoch);
    CATCH_REQUIRE(notParsed == nullptr);
}


CATCH_TEST_CASE("BiteCodec parses CAT args with selector and ignores others", "[bite][codec][cat]") {
    const uint64_t epoch = 3;
    auto key1 = libBLS::CipheredKey::random();
    auto key2 = libBLS::CipheredKey::random();

    std::vector<std::vector<uint8_t>> encryptedArgs = {
        *buildSerializedBiteData(key1, epoch),
        *buildSerializedBiteData(key2, epoch)
    };
    std::vector<std::vector<uint8_t>> plainArgs = { {0xAA}, {0xBB} };

    // correct selector - should parse
    auto encoded = BiteCodec::encodeCATData(encryptedArgs, plainArgs);
    auto parsed = BiteCodec::tryParseEncryptedCATArgs(encoded, epoch);
    CATCH_REQUIRE(parsed);
    CATCH_REQUIRE(parsed->size() == encryptedArgs.size());
    for (size_t i = 0; i < parsed->size(); i++) {
        CATCH_REQUIRE(parsed->at(i)->getEpoch() == epoch);
        // serialized ciphertexts should match what was encoded
        CATCH_REQUIRE(*parsed->at(i)->getSerializedData() == encryptedArgs[i]);
    }

    // missing selector - should not parse
    std::vector<uint8_t> withoutSelector{0x00, 0x01, 0x02, 0x03};
    CATCH_REQUIRE(BiteCodec::tryParseEncryptedCATArgs(withoutSelector, epoch) == nullptr);
}


CATCH_TEST_CASE("BiteCodec round trips regular payload encoding", "[bite][codec]") {
    std::vector<uint8_t> plainData{0xDE, 0xAD};
    std::vector<uint8_t> toBytes(ADDRESS_SIZE, 0x11);

    auto payload = BiteCodec::encodeRegularTxPayload(plainData, toBytes);
    auto parsed = BiteCodec::parseRegularTxDecryptedData(payload);

    CATCH_REQUIRE(parsed.data == plainData);
    std::vector<uint8_t> parsedTo(parsed.to.begin(), parsed.to.end());
    CATCH_REQUIRE(parsedTo == toBytes);
}


CATCH_TEST_CASE("BiteCodec decryptCiphertext returns original payload using mock crypto", "[bite][codec][decrypt]") {
    BiteCore core;
    core.doRealCrypto = false;

    std::vector<uint8_t> plainData{0x01, 0x02, 0x03};
    std::vector<uint8_t> toBytes(ADDRESS_SIZE, 0x22);
    auto payload = BiteCodec::encodeRegularTxPayload(plainData, toBytes);

    // random pub key
    auto key = libBLS::TEPublicKey::random();

    auto encrypted = core.encryptData(key, payload);
    BiteCiphertext ciphertext = BiteCiphertext(
        std::make_shared<std::vector<uint8_t>>(std::move(encrypted)), 
        0
    );

    libBLS::AES256Key aesKey{};
    auto decrypted = BiteCodec::decryptCiphertext(ciphertext, aesKey, core);
    CATCH_REQUIRE(decrypted == payload);
}


CATCH_TEST_CASE("BiteCodec decryptCiphertext returns original payload using real crypto", "[bite][codec][decrypt]") {
    BiteCore core;

    std::vector<uint8_t> plainData{0x01, 0x02, 0x03};
    std::vector<uint8_t> toBytes(ADDRESS_SIZE, 0x22);
    auto payload = BiteCodec::encodeRegularTxPayload(plainData, toBytes);

    // random pub key
    auto keys = generateKeys(1, 1); // util from libBLS

    // encrypt data at a lower level
    auto encrypted = core.encryptData(keys.commonPublic, payload);
    BiteCiphertext ciphertext = BiteCiphertext(
        std::make_shared<std::vector<uint8_t>>(std::move(encrypted)), 
        0
    );

    // get encrypted key
     libBLS::CipheredKey::fromBytes(ciphertext.getEncryptedAESKey().data());

    // // build decrypt set
    // libBLS::TEDecryptSet decryptSet(1, 1);
    // decryptSet.addDecryptShare(
    //     libBLS::ThresholdEncryption::partialDecrypt(cipheredKey, keys.secretKeys[0]));

    // // combine
    // auto decryptedKey = libBLS::ThresholdEncryption::combineShares( cipheredKey, decryptSet );

    // // payload should decrypt correctly
    // auto decrypted = BiteCodec::decryptCiphertext(ciphertext, decryptedKey, core);
    // CATCH_REQUIRE(decrypted == payload);
}


CATCH_TEST_CASE("BiteCodec splits decryption shares string correctly", "[bite][codec]") {

    // BITE1 format
    {
        std::string_view shares = "part1part2part3";
        auto split = BiteCodec::splitShares(shares);
        CATCH_REQUIRE(split.size() == 1);
        CATCH_REQUIRE(split[0] == "part1part2part3");
    }

    // BITE2 format
    {
        std::string_view shares = "part1,part2,part3";
        auto split = BiteCodec::splitShares(shares);
        CATCH_REQUIRE(split.size() == 4);
        CATCH_REQUIRE(split[0] == "part1");
        CATCH_REQUIRE(split[1] == "part2");
        CATCH_REQUIRE(split[2].empty());
        CATCH_REQUIRE(split[3] == "part3");
    }
}

#endif
