#include "thirdparty/catch.hpp"

#ifdef BITE

#include "crypto/TransactionCiphertexts.h"
#include "unittests/bite/BiteTestUtils.h"
#include "libBLS/test/utils.h"

using namespace BiteTestUtils;

namespace {

ptr<BiteCiphertext> makeValidCiphertext(uint64_t epoch) {
    auto serialized = std::make_shared<std::vector<uint8_t>>(buildBITE1EpochedData(
        {0x01, 0x02, 0x03},
        std::vector<uint8_t>(ADDRESS_SIZE, 0xAA),
        epoch,
        libBLS::TEPublicKey::random()
    ));
    return std::make_shared<BiteCiphertext>(serialized, epoch);
}

ptr<BiteCiphertext> makeValidCiphertextWithKey(uint64_t epoch, const libBLS::TEPublicKey& pk) {
    auto serialized = std::make_shared<std::vector<uint8_t>>(buildBITE1EpochedData(
        {0x01, 0x02, 0x03},
        std::vector<uint8_t>(ADDRESS_SIZE, 0xAA),
        epoch,
        pk
    ));
    return std::make_shared<BiteCiphertext>(serialized, epoch);
}

}  // namespace


CATCH_TEST_CASE("TransactionCiphertexts stores and returns CAT AAD", "[crypto][transactionciphertexts][cat][aad]") {
    const uint64_t epoch = 21;
    auto keys = generateKeys(1, 1);
    
    // Create ciphertexts for CAT transaction
    std::vector<ptr<BiteCiphertext>> ciphertexts;
    ciphertexts.push_back(makeValidCiphertextWithKey(epoch, keys.commonPublic));
    ciphertexts.push_back(makeValidCiphertextWithKey(epoch, keys.commonPublic));
    
    // Create with SC address AAD
    AddressBytes scAddress{};
    std::fill(scAddress.begin(), scAddress.end(), 0xDE);
    
    TransactionCiphertexts txCiphertexts(ciphertexts, scAddress);
    
    CATCH_REQUIRE(txCiphertexts.isCTX());
    CATCH_REQUIRE(txCiphertexts.count() == 2);
    
    auto aad = txCiphertexts.getScAddressAadTE();
    CATCH_REQUIRE(aad.has_value());
    CATCH_REQUIRE(*aad == scAddress);
}


CATCH_TEST_CASE("TransactionCiphertexts regular tx has no AAD", "[crypto][transactionciphertexts][aad]") {
    const uint64_t epoch = 22;
    
    // Create single ciphertext for regular BITE1 transaction
    auto ciphertext = makeValidCiphertext(epoch);
    
    TransactionCiphertexts txCiphertexts(ciphertext);
    
    CATCH_REQUIRE_FALSE(txCiphertexts.isCTX());
    CATCH_REQUIRE(txCiphertexts.count() == 1);
    
    auto aad = txCiphertexts.getScAddressAadTE();
    CATCH_REQUIRE_FALSE(aad.has_value());
}


CATCH_TEST_CASE("TransactionCiphertexts CAT with empty ciphertexts still has AAD", "[crypto][transactionciphertexts][cat][aad]") {
    // Create CAT with SC address but no ciphertexts (plain args only)
    AddressBytes scAddress{};
    std::fill(scAddress.begin(), scAddress.end(), 0xAB);
    
    std::vector<ptr<BiteCiphertext>> emptyCiphertexts;
    TransactionCiphertexts txCiphertexts(emptyCiphertexts, scAddress);
    
    CATCH_REQUIRE(txCiphertexts.isCTX());
    CATCH_REQUIRE(txCiphertexts.count() == 0);
    
    auto aad = txCiphertexts.getScAddressAadTE();
    CATCH_REQUIRE(aad.has_value());
    CATCH_REQUIRE(*aad == scAddress);
}

#endif // BITE
