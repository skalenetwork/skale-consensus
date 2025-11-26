#ifdef BITE2

#include "thirdparty/catch.hpp"

#include "SkaleCommon.h"
#include "bite/BiteManager.h"
#include "bite/BiteCiphertext.h"
#include "datastructures/Transaction.h"
#include "rlp/EthTransactionEncoder.h"
#include "rlp/RLPStream.h"

#include <vector>

namespace {

// Build a signed Ethereum transaction whose data field contains a BITE2 CAT
// payload (function selector + RLP(ciphertexts, plaintexts)).
// The ciphertext bytes are dummy but large enough to satisfy parsing checks.
ptr<Transaction> makeCatTransaction(std::size_t ciphertextCount) {
    constexpr uint64_t kEpoch = 0;

    // Prepare fake ciphertext blobs
    std::vector<std::vector<uint8_t>> ciphertextsSerialized;
    ciphertextsSerialized.reserve(ciphertextCount);
    for (std::size_t i = 0; i < ciphertextCount; ++i) {
        // Build a dummy encrypted payload large enough to satisfy BiteCiphertext checks.
        std::vector<uint8_t> rawCipher(
            BITE_ENCRYPTED_AES_KEY_LEN + BITE_TE_RANDOM_LEN + ADDRESS_SIZE + 1,
            static_cast<uint8_t>(i + 1));
        rawCipher[0] = 1;  // number of AES keys in payload
        auto cipherBytes = std::make_shared<std::vector<uint8_t>>(std::move(rawCipher));
        BiteCiphertext ct(cipherBytes, kEpoch);
        ciphertextsSerialized.emplace_back(*ct.getSerializedData());
    }

    // RLP encode ciphertext and plaintext lists
    RLPStream ciphertextStream;
    for (const auto &cipher : ciphertextsSerialized) {
        ciphertextStream << cipher;
    }
    RLPStream plaintextStream;
    plaintextStream << std::vector<uint8_t>{0x01, 0x02};  // dummy plaintext args

    RLPStream args;
    args << ciphertextStream << plaintextStream;

    // Build an eth tx with CAT payload in data field
    auto ethTx = EthTransactionEncoder::generateSampleTx();
    ethTx->data.clear();
    ethTx->data.insert(
        ethTx->data.end(),
        BITE_FUNCTION_SELECTOR_AS_BYTE_ARRAY,
        BITE_FUNCTION_SELECTOR_AS_BYTE_ARRAY + BITE_FUNCTION_SELECTOR_SIZE_BYTES
    );
    auto encodedArgs = args.encode();
    ethTx->data.insert(ethTx->data.end(), encodedArgs.begin(), encodedArgs.end());

    auto signedTx = EthTransactionEncoder::signAndEncodeTx(ethTx);
    auto raw = std::make_shared<std::vector<uint8_t>>(*signedTx);
    return Transaction::deserialize(raw, 0, raw->size(), false);
}

}  // namespace

CATCH_TEST_CASE("CAT transaction is detected and parsed", "[bite2][cat][rlp]") {
    auto tx = makeCatTransaction(/*ciphertextCount=*/3);

    auto catArgs = BiteManager::tryGetEncryptedCATArgs(tx, /*epoch*/ 0);

    CATCH_REQUIRE(catArgs);
    CATCH_REQUIRE(catArgs->size() == 3);
}

CATCH_TEST_CASE("Missing CAT selector prevents CAT parsing", "[bite2][cat][rlp]") {
    auto tx = makeCatTransaction(/*ciphertextCount=*/2);

    // Corrupt the selector so the parser should bail out
    auto corrupted = std::make_shared<std::vector<uint8_t>>(*tx->getData());
    corrupted->at(0) ^= 0xFF;
    auto badTx = Transaction::deserialize(corrupted, 0, corrupted->size(), false);

    auto catArgs = BiteManager::tryGetEncryptedCATArgs(badTx, /*epoch*/ 0);

    CATCH_REQUIRE_FALSE(catArgs);
}

#endif  // BITE2
