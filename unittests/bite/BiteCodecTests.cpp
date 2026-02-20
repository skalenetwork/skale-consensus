#include "thirdparty/catch.hpp"

#ifdef BITE

#include "bite/BiteCodec.h"
#include "bite/BiteCore.h"
#include "bite/Constants.h"
#include "exceptions/InvalidStateException.h"
#include "libBLS/threshold_encryption/threshold_encryption.h"
#include "libBLS/test/utils.h"

#include "BiteTestUtils.h"

using namespace BiteTestUtils;

CATCH_TEST_CASE("BiteCodec parses BITE1 transactions correctly", "[bite][codec]") {
    const uint64_t epoch = 7;
    auto serialized = std::make_shared<std::vector<uint8_t>>(buildBITE1EpochedData(
        {}, // empty payload
        std::vector<uint8_t>(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE),
        epoch,
        libBLS::TEPublicKey::random()
    ));

    // correct address - should parse
    auto biteAddress = std::vector<uint8_t>(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE);
    auto parsed = BiteCodec::tryParseEncryptedRegularTxFields(biteAddress, serialized, epoch);
    CATCH_REQUIRE(parsed);
    CATCH_REQUIRE(parsed->getEpoch() == epoch);

    // wrong address - should not parse
    std::vector<uint8_t> wrongTo(ADDRESS_SIZE, 0x00);
    auto notParsed = BiteCodec::tryParseEncryptedRegularTxFields(wrongTo, serialized, epoch);
    CATCH_REQUIRE(notParsed == nullptr);
}

CATCH_TEST_CASE("BiteCodec encodes/decodes epoched BITE data", "[bite][codec][epoched]") {
    const uint64_t epoch = 11;

    // build encrypted data
    std::vector<uint8_t> encryptedPayload(BITE_ENCRYPTED_AES_KEY_LEN + 16, 0xAB);
    auto keyPlusEncrypted = buildBITE1EncryptedData(
        encryptedPayload,
        std::vector<uint8_t>(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE),
        libBLS::TEPublicKey::random()
    );

    auto encoded = BiteCodec::encodeEpochedBiteData(keyPlusEncrypted, epoch);
    auto decoded = BiteCodec::decodeEpochedBiteData(encoded);

    CATCH_REQUIRE(decoded.epochId == epoch);
    CATCH_REQUIRE(*decoded.keyPlusEncryptedData == keyPlusEncrypted);
}

CATCH_TEST_CASE("BiteCodec decodeEpochedBiteData rejects malformed data", "[bite][codec][epoched][invalid]") {
    // not an RLP list of two elements
    std::vector<uint8_t> malformed{0x01, 0x02, 0x03};
    CATCH_REQUIRE_THROWS_AS(BiteCodec::decodeEpochedBiteData(malformed), InvalidStateException);
}

CATCH_TEST_CASE("BiteCodec enforces epoch when parsing BITE1 data", "[bite][codec][epoch]") {
    const uint64_t epoch = 9;
    auto serialized = std::make_shared<std::vector<uint8_t>>(buildBITE1EpochedData(
        {},
        std::vector<uint8_t>(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE),
        epoch,
        libBLS::TEPublicKey::random()
    ));
    auto biteAddress = std::vector<uint8_t>(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE);

    // matching epoch works
    auto parsed = BiteCodec::tryParseEncryptedRegularTxFields(biteAddress, serialized, epoch);
    CATCH_REQUIRE(parsed);
    CATCH_REQUIRE(parsed->getEpoch() == epoch);

    // mismatching epoch throws
    CATCH_REQUIRE_THROWS_AS(
        BiteCodec::tryParseEncryptedRegularTxFields(biteAddress, serialized, epoch + 1),
        InvalidStateException);
}

#ifdef BITE2
CATCH_TEST_CASE("BiteCodec parses CAT args with selector and ignores others", "[bite][codec][cat]") {
    const uint64_t epoch = 3;

    std::vector<std::vector<uint8_t>> encryptedArgs = {
        buildBITE1EpochedData(
            {},
            std::vector<uint8_t>(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE),
            epoch,
            libBLS::TEPublicKey::random()
        ),
        buildBITE1EpochedData(
            {},
            std::vector<uint8_t>(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE),
            epoch,
            libBLS::TEPublicKey::random()
        )
    };
    std::vector<std::vector<uint8_t>> plainArgs = { {0xAA}, {0xBB} };

    // correct selector - should parse
    auto encoded = BiteCodec::encodeCTXData(encryptedArgs, plainArgs);
    auto parsed = BiteCodec::tryParseEncryptedCTXArgs(encoded, epoch);
    CATCH_REQUIRE(parsed);
    CATCH_REQUIRE(parsed->size() == encryptedArgs.size());
    for (size_t i = 0; i < parsed->size(); i++) {
        CATCH_REQUIRE(parsed->at(i)->getEpoch() == epoch);
        // serialized ciphertexts should match what was encoded
        CATCH_REQUIRE(*parsed->at(i)->getSerializedData() == encryptedArgs[i]);
    }

    // missing selector - should not parse
    std::vector<uint8_t> withoutSelector{0x00, 0x01, 0x02, 0x03};
    CATCH_REQUIRE(BiteCodec::tryParseEncryptedCTXArgs(withoutSelector, epoch) == nullptr);
}

CATCH_TEST_CASE("BiteCodec enforces epoch for CAT args", "[bite][codec][cat][epoch]") {
    const uint64_t epoch = 4;
    std::vector<std::vector<uint8_t>> encryptedArgs = {
        buildBITE1EpochedData(
            {},
            std::vector<uint8_t>(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE),
            epoch,
            libBLS::TEPublicKey::random()
        ),
        buildBITE1EpochedData(
            {},
            std::vector<uint8_t>(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE),
            epoch,
            libBLS::TEPublicKey::random()
        )
    };
    std::vector<std::vector<uint8_t>> plainArgs = { {0xAA}, {0xBB} };

    auto encoded = BiteCodec::encodeCTXData(encryptedArgs, plainArgs);

    // matching epoch works
    auto parsed = BiteCodec::tryParseEncryptedCTXArgs(encoded, epoch);
    CATCH_REQUIRE(parsed);
    CATCH_REQUIRE(parsed->size() == encryptedArgs.size());

    // mismatching epoch throws when constructing BiteCiphertext for args
    CATCH_REQUIRE_THROWS_AS(
        BiteCodec::tryParseEncryptedCTXArgs(encoded, epoch + 1),
        InvalidStateException);
}
#endif

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

    // encode into bite epoched data
    auto biteEpochedData = BiteCodec::encodeEpochedBiteData(encrypted, 0);

    BiteCiphertext biteCiphertext = BiteCiphertext(
        std::make_shared<std::vector<uint8_t>>(biteEpochedData), 
        0
    );

    libBLS::AES256Key aesKey{};
    auto decrypted = BiteCodec::decryptCiphertext(biteCiphertext, aesKey, core);
    CATCH_REQUIRE(decrypted == payload);
}


CATCH_TEST_CASE("BiteCodec decryptCiphertext returns original payload using real crypto", "[bite][codec][decrypt]") {
    BiteCore core;

    std::vector<uint8_t> plainData{0x01, 0x02, 0x03};
    std::vector<uint8_t> toBytes(ADDRESS_SIZE, 0x22);
    auto payload = BiteCodec::encodeRegularTxPayload(plainData, toBytes);

    // random pub key
    auto keys = generateKeys(1, 1); // util from libBLS

    // encrypt data at a lower level and keep both the TE ciphertext object and its bytes
    libBLS::Ciphertext teCiphertext = libBLS::ThresholdEncryption::encrypt(payload, keys.commonPublic);
    libBLS::CipheredKey cipheredKey = teCiphertext.keys.at(0);
    auto encryptedBytes = teCiphertext.toBytes();

    // encode into bite epoched data
    auto biteEpochedData = BiteCodec::encodeEpochedBiteData(encryptedBytes, 0);

    // build ciphertext from epoched data
    BiteCiphertext biteCiphertext = BiteCiphertext(
        std::make_shared<std::vector<uint8_t>>(biteEpochedData), 
        0
    );

    // build decrypt set
    libBLS::TEDecryptSet decryptSet(1, 1);
    decryptSet.addDecryptShare(
        libBLS::ThresholdEncryption::partialDecrypt(cipheredKey, keys.secretKeys[0]));

    // combine
    auto decryptedKey = libBLS::ThresholdEncryption::combineShares( cipheredKey, decryptSet );

    // payload should decrypt correctly
    auto decrypted = BiteCodec::decryptCiphertext(biteCiphertext, decryptedKey, core);
    CATCH_REQUIRE(decrypted == payload);
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
        CATCH_REQUIRE(split.size() == 3);
        CATCH_REQUIRE(split[0] == "part1");
        CATCH_REQUIRE(split[1] == "part2");
        CATCH_REQUIRE(split[2] == "part3");
    }
}

#endif
