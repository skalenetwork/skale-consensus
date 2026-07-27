#include "thirdparty/catch.hpp"

#ifdef BITE

#include <folly/executors/CPUThreadPoolExecutor.h>
#include <array>
#include <string>

#include "BiteTestUtils.h"
#include "bite/BiteEngine.h"
#include "crypto/ConsensusAESKeyDecryptionShare.h"
#include "crypto/ConsensusAESKeyDecryptionShareSet.h"
#include "crypto/DecryptedAESKeyList.h"
#include "crypto/AESKeyDecryptionShareList.h"
#include "crypto/MockupAESKeyDecryptionShare.h"
#include "crypto/MockupAESKeyDecryptionShareSet.h"
#include "crypto/AESKeyDecryptionShare.h"
#include "bite/serde/BiteAESKeySerializer.h"
#include <flatbuffers/flatbuffers.h>
#include "datastructures/TransactionCiphertextsMap.h"
#include "datastructures/TransactionList.h"
#include "libBLS/test/utils.h"

using namespace BiteTestUtils;

// ================================= BiteEngine Utils ================================= //

namespace {

BiteEngine makeEngine(bool realCrypto, BiteConfig cfg = BiteConfig{2, 3}) {
    BiteCore core;
    core.doRealCrypto = realCrypto;
    return BiteEngine(core, cfg);
}

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

std::vector<ptr<BiteCiphertext>> makeCatCiphertextsWithKey(
    uint64_t epoch,
    const libBLS::TEPublicKey& pk,
    size_t count
) {
    std::vector<ptr<BiteCiphertext>> out;
    for (size_t i = 0; i < count; ++i) {
        auto serialized = std::make_shared<std::vector<uint8_t>>(buildBITE1EpochedData(
            {static_cast<uint8_t>(i)},
            std::vector<uint8_t>(ADDRESS_SIZE, 0xAB),
            epoch,
            pk
        ));
        out.push_back(std::make_shared<BiteCiphertext>(serialized, epoch));
    }
    return out;
}

ptr<BiteCiphertext> makeInvalidParseCiphertext(uint64_t epoch) {
    // Build epoched data with a malformed encrypted key payload (junk bytes)
    // data field does not parse into a valid Ciphertext
    std::vector<uint8_t> junk(BITE_ENCRYPTED_AES_KEY_LEN + 4, 0xEE);
    junk[0] = 0x01; // number of encrypted keys
    auto serialized = BiteCodec::encodeEpochedBiteData(junk, epoch);
    return std::make_shared<BiteCiphertext>(
        std::make_shared<std::vector<uint8_t>>(serialized),
        epoch
    );
}

ptr<BiteCiphertext> makeInvalidSemanticCiphertext(uint64_t epoch) {
    auto payload = BiteCodec::encodeRegularTxPayload(
        {0x1, 0x2, 0x3}, 
        std::vector<uint8_t>(ADDRESS_SIZE, 0xBB));

    auto ciphertext = libBLS::ThresholdEncryption::encrypt(payload, libBLS::TEPublicKey::random());
    ciphertext.keys[0].U = libBLS::algebra::G2Point::random(); // invalidate the ciphertext

    auto biteEncodedData = BiteCodec::encodeEpochedBiteData(
        ciphertext.toBytes(), epoch
    );

    return std::make_shared<BiteCiphertext>(
        std::make_shared<std::vector<uint8_t>>(biteEncodedData),
        epoch
    );
}

libBLS::Ciphertext getCiphertextFromBITE1Transaction(ptr<Transaction> tx, epoch_id epoch) {
    auto biteCiphertext = BiteEngine::tryGetEncryptedRegularTxFields(tx, epoch);
    CATCH_CHECK(biteCiphertext != nullptr);
    return libBLS::Ciphertext::fromBytes(
        *biteCiphertext->getSerializedData(), true /* validate */);
}

libBLS::AES256Key runThresholdEncryptionAndCombineShares(
    const libBLS::CipheredKey& cipheredKey, const keys& keys, size_t threshold, size_t total) {
        libBLS::TEDecryptSet decryptSet(threshold, total);
    for (size_t i = 0; i < threshold; ++i) {
        auto share = libBLS::ThresholdEncryption::partialDecrypt(
            cipheredKey, keys.secretKeys[i]);
        decryptSet.addDecryptShare(share);
    }

    return libBLS::ThresholdEncryption::combineShares(cipheredKey, decryptSet);
}

DecryptedAESKeys getDecryptedAESKeysForTransaction(
    const ptr<Transaction>& tx,
    const keys& keySet,
    size_t threshold,
    size_t total,
    epoch_id epoch
) {
    std::vector<ptr<BiteCiphertext>> ciphertexts;

    // mimic engine: CAT first (if enabled), otherwise regular BITE1
    if (auto ctxArgs = BiteEngine::tryGetEncryptedCTXArgs(tx, epoch)) {
        ciphertexts.insert(ciphertexts.end(), ctxArgs->begin(), ctxArgs->end());
    } else
    if (auto regular = BiteEngine::tryGetEncryptedRegularTxFields(tx, epoch)) {
        ciphertexts.push_back(regular);
    }

    CATCH_CHECK(!ciphertexts.empty());

    DecryptedAESKeys decryptedKeys;

    for (const auto& biteCiphertext : ciphertexts) {
        libBLS::Ciphertext teCiphertext =
            libBLS::Ciphertext::fromBytes(*biteCiphertext->getKeyPlusEncryptedData(), true /* validate */);

        auto key = teCiphertext.keys.at(0);
        auto decryptedKey = runThresholdEncryptionAndCombineShares(key, keySet, threshold, total);
        decryptedKeys.push_back(DecryptedAESKey(decryptedKey));
    }

    return decryptedKeys;
}


DecryptedAESKeys makeSingleDecryptedKey(uint8_t fill = 0x00) {
    std::array<uint8_t, BITE_AES_KEY_LEN> key{};
    key.fill(fill);
    DecryptedAESKeys keys;
    keys.push_back(DecryptedAESKey(key));
    return keys;
}

DecryptedAESKeys makeDecryptedKeys(size_t count, uint8_t start = 0x00) {
    DecryptedAESKeys keys;
    for (size_t i = 0; i < count; ++i) {
        std::array<uint8_t, BITE_AES_KEY_LEN> key{};
        key.fill(static_cast<uint8_t>(start + i));
        keys.push_back(DecryptedAESKey(key));
    }
    return keys;
}

std::vector<uint8_t> randomData(size_t size) {
    std::vector<uint8_t> data(size);
    for (size_t i = 0; i < size; ++i) {
        data[i] = static_cast<uint8_t>(rand() % 256);
    }
    return data;
}

std::vector<uint8_t> randomAddress() {
    return randomData(ADDRESS_SIZE);
}

ptr<AESKeyDecryptionShareList> makeMockupShareList(
    block_id blockId,
    schain_index decryptorIdx,
    const TransactionCiphertextsMap& txCiphertexts
) {
    auto list = std::make_shared<AESKeyDecryptionShareList>(blockId, /*proposer*/1, decryptorIdx);
    for (auto&& [txIdx, txCts] : txCiphertexts) {
    auto shares = std::make_shared<AESKeyDecryptionShares>();
    for (auto& encKey : txCts->getCiphertexts()) {
        // mockupDecrypt expects non-const reference
        auto copy = encKey;
        shares->push_back(MockupAESKeyDecryptionShare::mockupDecrypt(copy, decryptorIdx));
    }
    list->addShares(txIdx, shares);
}
return list;
}

ptr<AESKeyDecryptionShareList> makeConsensusShareList(
    block_id blockId,
    schain_index decryptorIdx,
    const TransactionCiphertextsMap& txCiphertexts,
    const keys& keySet
) {
    auto list = std::make_shared<AESKeyDecryptionShareList>(blockId, /*proposer*/1, decryptorIdx);
    bool validate = true;
    for (auto&& [txIdx, txCts] : txCiphertexts) {
        auto shares = std::make_shared<AESKeyDecryptionShares>();
        for (auto& encKey : txCts->getCiphertexts()) {
            auto cipheredKey = libBLS::CipheredKey::fromBytes(encKey.data(), validate);
            auto teShare = libBLS::ThresholdEncryption::partialDecrypt(
                cipheredKey,
                keySet.secretKeys[static_cast<size_t>(decryptorIdx - 1)]
            );
            shares->push_back(std::make_shared<ConsensusAESKeyDecryptionShare>(
                std::make_shared<libBLS::TEDecryptionShare>(teShare),
                decryptorIdx,
                false));
        }
        list->addShares(txIdx, shares);
    }
    return list;
}

}  // namespace


// ================================= BiteEngine tests ================================= //


CATCH_TEST_CASE("BiteEngine returns cached BITE ciphertext", "[bite][engine]") {
    BiteCore core;
    core.doRealCrypto = false;
    BiteEngine engine(core, BiteConfig{});

    ptr< Transaction > tx = std::make_shared<Transaction>(std::make_shared<std::vector<uint8_t>>(1, 0x01), false);

    auto randomCiphertext = buildBITE1EpochedData(
        {0xDE, 0xAD, 0xBE, 0xEF},
        std::vector<uint8_t>(20, 0x42),
        1,
        libBLS::TEPublicKey::random()
    );

    auto cached = std::make_shared<BiteCiphertext>(
        std::make_shared<std::vector<uint8_t>>(randomCiphertext),
        (epoch_id)1);

    tx->setRegularTxEncryptedData(cached);

    auto parsed = BiteEngine::tryGetEncryptedRegularTxFields(tx, 1);
    CATCH_REQUIRE(parsed == cached);
}


CATCH_TEST_CASE("BiteEngine parses and caches BITE ciphertext from transaction data", "[bite][engine][parse][cache]") {
    const uint64_t epoch = 2;

    auto tx = buildBite1Transaction(
        {0x0A, 0x0B, 0x0C},
        std::vector<uint8_t>(ADDRESS_SIZE, 0x33),
        epoch,
        libBLS::TEPublicKey::random()
    );

    auto first = BiteEngine::tryGetEncryptedRegularTxFields(tx, epoch);
    CATCH_REQUIRE(first);

    // second call should return the cached instance
    auto second = BiteEngine::tryGetEncryptedRegularTxFields(tx, epoch);
    CATCH_REQUIRE(second == first);
    CATCH_REQUIRE(tx->getRegularTxEncryptedData() == first);
}


CATCH_TEST_CASE("BiteEngine parses BITE txs and reports failures", "[bite][engine][parse]") {
    const uint64_t epoch = 4;

    // 2 good BITE1 transactions
    auto tx1 = buildBite1Transaction(
        {0xAA, 0xBB, 0xCC},
        std::vector<uint8_t>(20, 0x10),
        epoch,
        libBLS::TEPublicKey::random()
    );

    auto tx2 = buildBite1Transaction(
        {0x11, 0x22, 0x33},
        std::vector<uint8_t>(20, 0x20),
        epoch,
        libBLS::TEPublicKey::random()
    );

    // bad tx - invalid BITE data
    auto tx = EthTransactionEncoder::generateSampleTx();

    auto encodedBITEData = BiteCodec::encodeRegularTxPayload(
        {0xAA, 0xBB, 0xCC}, std::vector<uint8_t>(20, 0x10));

    BiteCore core{};
    auto encryptedData = core.encryptData(libBLS::TEPublicKey::random(), encodedBITEData);

    tx->to = std::vector<uint8_t>(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE);
    // corrupt data
    encryptedData[5] ^= 0xFF;
    tx->data = encryptedData;
    auto encoded = EthTransactionEncoder::signAndEncodeTx(tx);
    auto badTx = std::make_shared<Transaction>(encoded, false);    

    // create vector of transactions
    auto txVec = std::make_shared<std::vector<ptr<Transaction>>>();
    txVec->push_back(tx1);
    txVec->push_back(tx2);
    txVec->push_back(badTx);

    TransactionList txList(txVec);

    BiteRuntimeContext ctx{
        epoch, 
        nullptr
        , true
    };

    auto result = BiteEngine::parseAndCacheBITETransactions(txList, ctx);

    CATCH_REQUIRE(result.txsCiphertexts.size() == 2);
    CATCH_REQUIRE(result.txsCiphertexts.totalCiphertextCount() == 2);
    CATCH_REQUIRE(result.failedTransactions.size() == 1);  // one failed
    CATCH_REQUIRE(result.failedTransactions.front() == 2); // idx of the transaction that failed
}

CATCH_TEST_CASE("BiteEngine parse fails on epoch mismatch, when tx uses single ciphertext", "[bite][engine][parse][epoch]") {
    const uint64_t epochTx = 10;
    auto tx = buildBite1Transaction(
        {0x01},
        std::vector<uint8_t>(ADDRESS_SIZE, 0x11),
        epochTx,
        libBLS::TEPublicKey::random()
    );

    auto txVec = std::make_shared<std::vector<ptr<Transaction>>>();
    txVec->push_back(tx);
    TransactionList txList(txVec);

    BiteRuntimeContext ctx{epochTx + 1, nullptr
                          , true
                          };
    auto result = BiteEngine::parseAndCacheBITETransactions(txList, ctx);

    CATCH_REQUIRE(result.txsCiphertexts.size() == 0);
    CATCH_REQUIRE(result.failedTransactions.size() == 1);
    CATCH_REQUIRE(result.failedTransactions.front() == 0);
}

// CATCH_TEST_CASE("BiteEngine parse succeeds on epoch mismatch, if tx uses multiple ciphertexts, and epoch is +1", "[bite][codec][decrypt]") {
//     // TODO
// }

CATCH_TEST_CASE("BiteEngine ignores non-BITE transactions", "[bite][engine][parse][nonbite]") {
    auto tx = EthTransactionEncoder::generateSampleTx();
    tx->to = std::vector<uint8_t>(ADDRESS_SIZE, 0x00); // non-BITE address
    tx->data = {0x01, 0x02}; // arbitrary data
    auto encoded = EthTransactionEncoder::signAndEncodeTx(tx);

    auto txVec = std::make_shared<std::vector<ptr<Transaction>>>();
    txVec->push_back(std::make_shared<Transaction>(encoded, false));
    TransactionList txList(txVec);

    BiteRuntimeContext ctx{0, nullptr
                            , true
                          };
    auto result = BiteEngine::parseAndCacheBITETransactions(txList, ctx);

    CATCH_REQUIRE(result.txsCiphertexts.size() == 0);
    CATCH_REQUIRE(result.failedTransactions.empty());
}

CATCH_TEST_CASE("BiteEngine validateCiphertexts succeeds for valid inputs, when using 1 ciphertext per tx", "[bite][engine][validate]") {
    BiteEngine engine = makeEngine(true);
    TransactionCiphertextsMap map;
    BiteRuntimeContext runtimeContext;

    auto c1 = makeValidCiphertext(5);
    auto c2 = makeValidCiphertext(6);

    map.emplace(0, std::make_shared<TransactionCiphertexts>(c1));
    map.emplace(1, std::make_shared<TransactionCiphertexts>(c2));

    auto result = engine.validateCiphertexts(map, runtimeContext);
    CATCH_REQUIRE(result.allValid());
    CATCH_REQUIRE(result.invalidCiphertextIndices.empty());
    CATCH_REQUIRE(result.publicDecryptionValues.size() == map.totalCiphertextCount());
}

CATCH_TEST_CASE("BiteEngine validateCiphertexts invalid not parseable", "[bite][engine][validate][invalid]") {
    BiteEngine engine = makeEngine(true);
    TransactionCiphertextsMap map;

    auto valid = makeValidCiphertext(7);
    auto invalid = makeInvalidParseCiphertext(7);

    map.emplace(0, std::make_shared<TransactionCiphertexts>(valid));
    map.emplace(1, std::make_shared<TransactionCiphertexts>(invalid));

    BiteRuntimeContext runtimeContext;
    auto result = engine.validateCiphertexts(map, runtimeContext);
    CATCH_REQUIRE_FALSE(result.allValid());
    CATCH_REQUIRE(result.invalidCiphertextIndices.size() == 1);
    CATCH_REQUIRE(result.invalidCiphertextIndices.front() == 1);
    // check reason
    CATCH_REQUIRE(result.failureReasons.size() == 1);
    CATCH_REQUIRE(result.failureReasons.front().find("failed to parse") != std::string::npos);
    // if at least 1 invalid - no public values returned
    CATCH_REQUIRE(result.publicDecryptionValues.empty()); 
}


CATCH_TEST_CASE("BiteEngine validateCiphertexts invalid parseable but semantically wrong", "[bite][engine][validate]") {
    BiteEngine engine = makeEngine(true);
    TransactionCiphertextsMap map;

    auto valid = makeValidCiphertext(7);
    auto invalid = makeInvalidSemanticCiphertext(7);

    map.emplace(0, std::make_shared<TransactionCiphertexts>(valid));
    map.emplace(1, std::make_shared<TransactionCiphertexts>(invalid));

    BiteRuntimeContext runtimeContext;
    auto result = engine.validateCiphertexts(map, runtimeContext);
    CATCH_REQUIRE_FALSE(result.allValid());
    CATCH_REQUIRE(result.invalidCiphertextIndices.size() == 1);
    CATCH_REQUIRE(result.invalidCiphertextIndices.front() == 1);
    // check reason
    CATCH_REQUIRE(result.failureReasons.size() == 1);
    CATCH_REQUIRE(result.failureReasons.front().find("failed semantic validation") != std::string::npos);
    // if at least 1 invalid - no public values returned
    CATCH_REQUIRE(result.publicDecryptionValues.empty()); 
}

CATCH_TEST_CASE("BiteEngine validateCiphertexts empty map is valid", "[bite][engine][validate][empty]") {
    BiteEngine engine = makeEngine(true);
    TransactionCiphertextsMap map;
    BiteRuntimeContext runtimeContext;
    auto result = engine.validateCiphertexts(map, runtimeContext);
    CATCH_REQUIRE(result.allValid());
    CATCH_REQUIRE(result.invalidCiphertextIndices.empty());
    CATCH_REQUIRE(result.publicDecryptionValues.empty());
}

CATCH_TEST_CASE("BiteEngine validateCiphertexts parse failure only", "[bite][engine][validate][invalid]") {
    BiteEngine engine = makeEngine(true);
    TransactionCiphertextsMap map;

    auto badCipher = makeInvalidParseCiphertext(5);
    map.emplace(0, std::make_shared<TransactionCiphertexts>(badCipher));

    BiteRuntimeContext runtimeContext;
    auto result = engine.validateCiphertexts(map, runtimeContext);
    CATCH_REQUIRE_FALSE(result.allValid());
    CATCH_REQUIRE(result.invalidCiphertextIndices.size() == 1);
    CATCH_REQUIRE(result.invalidCiphertextIndices.front() == 0);
    CATCH_REQUIRE(result.publicDecryptionValues.empty());
}

CATCH_TEST_CASE("BiteEngine validateCiphertexts single invalid ciphertext invalidates whole transaction", "[bite][engine][validate][invalid]") {
    BiteEngine engine = makeEngine(true);
    TransactionCiphertextsMap map;

    // tx 0 - valid ciphertexts
    std::vector<ptr<BiteCiphertext>> tx0Ciphertexts;
    tx0Ciphertexts.push_back( makeValidCiphertext(8) );
    tx0Ciphertexts.push_back( makeValidCiphertext(8) );
    map.emplace(0, std::make_shared<TransactionCiphertexts>(tx0Ciphertexts));
    
    // tx1 - second ciphertext invalid - not parseable
    std::vector<ptr<BiteCiphertext>> tx1Ciphertexts;
    tx1Ciphertexts.push_back( makeValidCiphertext(8) );
    tx1Ciphertexts.push_back( makeInvalidParseCiphertext(8) );
    map.emplace(1, std::make_shared<TransactionCiphertexts>(tx1Ciphertexts));

    // tx2 - second ciphertext invalid - semantically wrong
    std::vector<ptr<BiteCiphertext>> tx2Ciphertexts;
    tx2Ciphertexts.push_back( makeValidCiphertext(8) );
    tx2Ciphertexts.push_back( makeInvalidSemanticCiphertext(8) );
    map.emplace(2, std::make_shared<TransactionCiphertexts>(tx2Ciphertexts));

    BiteRuntimeContext runtimeContext;
    auto result = engine.validateCiphertexts(map, runtimeContext);
    CATCH_REQUIRE_FALSE(result.allValid());
    CATCH_REQUIRE(result.invalidCiphertextIndices.size() == 2);
    CATCH_REQUIRE(result.invalidCiphertextIndices.front() == 1);
    CATCH_REQUIRE(result.invalidCiphertextIndices.back() == 2);
    // check reasons
    CATCH_REQUIRE(result.failureReasons.size() == 2);
    CATCH_REQUIRE(result.failureReasons[0].find("failed to parse") != std::string::npos);
    CATCH_REQUIRE(result.failureReasons[1].find("failed semantic validation") != std::string::npos);
    // if at least 1 invalid - no public values returned
    CATCH_REQUIRE(result.publicDecryptionValues.empty()); 
}





CATCH_TEST_CASE("BiteEngine decrypts regular BITE transactions in parallel - single transaction", "[bite][engine][decrypt]") {
    const uint64_t epoch = 6;
    BiteCore core;
    core.doRealCrypto = true;
    BiteEngine engine(core, BiteConfig{});

    std::vector<uint8_t> toBytes(ADDRESS_SIZE, 0x33);
    std::vector<uint8_t> plainData{0x0A, 0x0B, 0x0C};

    auto keys = generateKeys(1, 1); // util from libBLS

    auto tx = buildBite1Transaction(
        plainData, 
        toBytes, 
        epoch, 
        keys.commonPublic
    );

    DecryptedAESKeys decryptedKeys = getDecryptedAESKeysForTransaction(
        tx, keys, 1, 1, epoch);

    auto txVec = std::make_shared<std::vector<ptr<Transaction>>>(1, tx);
    TransactionList txList(txVec);

    DecryptedAESKeyList aesKeys;
    aesKeys.addKeys(0, decryptedKeys);

    BiteRuntimeContext ctx{epoch, std::make_shared<folly::CPUThreadPoolExecutor>(2)
                            , true
                          };
    auto decrypted = engine.decryptTransactionsListInParallel(txList, aesKeys, ctx);

    CATCH_REQUIRE(decrypted.regularTxsMap);
    CATCH_REQUIRE(decrypted.regularTxsMap->size() == 1);
    auto it = decrypted.regularTxsMap->find(0);
    CATCH_REQUIRE(it != decrypted.regularTxsMap->end());
    CATCH_REQUIRE(it->second.has_value());
    CATCH_REQUIRE(it->second->data == plainData);
    std::vector<uint8_t> parsedTo(it->second->to.begin(), it->second->to.end());
    CATCH_REQUIRE(parsedTo == toBytes);
}


CATCH_TEST_CASE("BiteEngine decrypts regular BITE transactions in parallel - multiple BITE1 transactions", "[bite][engine][decrypt]") {
    const size_t threshold = 15;
    const size_t total = 22;
    const uint64_t epoch = 6;
    BiteCore core;
    core.doRealCrypto = true;
    BiteEngine engine(core, BiteConfig{});

    auto keys = generateKeys(threshold, total); // util from libBLS

    auto txsVec = std::make_shared<std::vector<ptr<Transaction>>>();
    DecryptedAESKeyList decryptedKeysList;

    std::vector<std::vector<uint8_t>> plainDatas;
    std::vector<std::vector<uint8_t>> toAddresses;

    // 30 txs block of BITE1 txs
    const size_t numTxs = 30;
    for (size_t i = 0; i < numTxs; ++i) {
        plainDatas.push_back(randomData(rand() % 150));
        toAddresses.push_back(randomAddress());
        
        txsVec->push_back(buildBite1Transaction(
            plainDatas.back(), 
            toAddresses.back(), 
            epoch, 
            keys.commonPublic
        ));

        DecryptedAESKeys decryptedKeys = getDecryptedAESKeysForTransaction(
        txsVec->back(), keys, threshold, total, epoch);
        decryptedKeysList.addKeys(i, decryptedKeys);
    }

    TransactionList txList(txsVec);
    BiteRuntimeContext ctx{epoch, std::make_shared<folly::CPUThreadPoolExecutor>(2)
                            , true
                          };
    auto decrypted = engine.decryptTransactionsListInParallel(txList, decryptedKeysList, ctx);

    CATCH_REQUIRE(decrypted.regularTxsMap);
    CATCH_REQUIRE(decrypted.regularTxsMap->size() == numTxs);

    for (size_t i = 0; i < numTxs; ++i) {
        auto it = decrypted.regularTxsMap->find(i);
        CATCH_REQUIRE(it != decrypted.regularTxsMap->end());
        CATCH_REQUIRE(it->second.has_value());
        CATCH_REQUIRE(it->second->data == plainDatas[i]);
        std::vector<uint8_t> parsedTo(it->second->to.begin(), it->second->to.end());
        CATCH_REQUIRE(parsedTo == toAddresses[i]);
    }
}

CATCH_TEST_CASE("BiteEngine decrypts mixed BITE and plaintext transactions", "[bite][engine][decrypt][mixed]") {
    const uint64_t epoch = 8;
    BiteCore core;
    core.doRealCrypto = true;
    BiteEngine engine(core, BiteConfig{});

    auto keys = generateKeys(1, 1);
    auto biteTx = buildBite1Transaction(
        std::vector<uint8_t>{0x01, 0x02, 0x03},
        std::vector<uint8_t>(ADDRESS_SIZE, 0x55),
        epoch,
        keys.commonPublic
    );

    auto plainTx = EthTransactionEncoder::generateSampleTx();
    plainTx->to = std::vector<uint8_t>(ADDRESS_SIZE, 0x00);
    plainTx->data = {0x01, 0x02}; // arbitrary payload
    auto plainWrapped = std::make_shared<Transaction>(
        EthTransactionEncoder::signAndEncodeTx(plainTx),
        false);

    auto txVec = std::make_shared<std::vector<ptr<Transaction>>>();
    txVec->push_back(biteTx);       // idx 0
    txVec->push_back(plainWrapped); // idx 1
    TransactionList txList(txVec);

    DecryptedAESKeyList aesKeys;
    aesKeys.addKeys(0, getDecryptedAESKeysForTransaction(biteTx, keys, 1, 1, epoch));

    BiteRuntimeContext ctx{epoch, std::make_shared<folly::CPUThreadPoolExecutor>(2)
                            , true
                          };
    auto decrypted = engine.decryptTransactionsListInParallel(txList, aesKeys, ctx);

    CATCH_REQUIRE(decrypted.regularTxsMap);
    CATCH_REQUIRE(decrypted.regularTxsMap->size() == 1);
    auto it = decrypted.regularTxsMap->find(0);
    CATCH_REQUIRE(it != decrypted.regularTxsMap->end());
}

CATCH_TEST_CASE("BiteEngine decrypts only CAT transactions", "[bite][engine][decrypt][cat]") {
    const uint64_t epoch = 9;
    BiteCore core;
    core.doRealCrypto = true;
    BiteEngine engine(core, BiteConfig{});
    auto keys = generateKeys(1, 1);

    auto catTx1 = buildBite2Transaction(
        { {0x01, 0x02}, {0x03} },
        { {0xAA}, {0xBB} },
        epoch,
        keys.commonPublic
    );
    auto catTx2 = buildBite2Transaction(
        { {0x10}, {0x20}, {0x30} },
        { {0xCC}, {0xDD}, {0xEE} },
        epoch,
        keys.commonPublic
    );

    auto txVec = std::make_shared<std::vector<ptr<Transaction>>>();
    txVec->push_back(catTx1); // idx 0
    txVec->push_back(catTx2); // idx 1
    TransactionList txList(txVec);

    DecryptedAESKeyList aesKeys;
    aesKeys.addKeys(0, getDecryptedAESKeysForTransaction(catTx1, keys, 1, 1, epoch));
    aesKeys.addKeys(1, getDecryptedAESKeysForTransaction(catTx2, keys, 1, 1, epoch));

    BiteRuntimeContext ctx{epoch, std::make_shared<folly::CPUThreadPoolExecutor>(2), true};
    auto decrypted = engine.decryptTransactionsListInParallel(txList, aesKeys, ctx);

    CATCH_REQUIRE(decrypted.ctxTxsMap);
    CATCH_REQUIRE(decrypted.ctxTxsMap->size() == 2);
    CATCH_REQUIRE(decrypted.ctxTxsMap->count(0) == 1);
    CATCH_REQUIRE(decrypted.ctxTxsMap->count(1) == 1);
    CATCH_REQUIRE(decrypted.regularTxsMap->empty());
}

CATCH_TEST_CASE("BiteEngine decrypts mixed CAT and BITE transactions", "[bite][engine][decrypt][cat][mixed]") {
    const uint64_t epoch = 11;
    BiteCore core;
    core.doRealCrypto = true;
    BiteEngine engine(core, BiteConfig{});
    auto keys = generateKeys(1, 1);

    // 1 cat at start
    auto catTx = buildBite2Transaction(
        { {0x01}, {0x02} },
        { {0x10}, {0x20} },
        epoch,
        keys.commonPublic
    );

    // 1 BITE1
    auto biteTx = buildBite1Transaction(
        {0xAA},
        std::vector<uint8_t>(ADDRESS_SIZE, 0x44),
        epoch,
        keys.commonPublic
    );

    auto txVec = std::make_shared<std::vector<ptr<Transaction>>>();
    txVec->push_back(catTx);  // idx 0
    txVec->push_back(biteTx); // idx 1
    TransactionList txList(txVec);

    DecryptedAESKeyList aesKeys;
    aesKeys.addKeys(0, getDecryptedAESKeysForTransaction(catTx, keys, 1, 1, epoch));
    aesKeys.addKeys(1, getDecryptedAESKeysForTransaction(biteTx, keys, 1, 1, epoch));

    BiteRuntimeContext ctx{epoch, std::make_shared<folly::CPUThreadPoolExecutor>(2)
                            , true
                          };
    auto decrypted = engine.decryptTransactionsListInParallel(txList, aesKeys, ctx);

    // 1 cat decrypted
    CATCH_REQUIRE(decrypted.ctxTxsMap);
    CATCH_REQUIRE(decrypted.ctxTxsMap->size() == 1);
    // 1 regular decrypted
    CATCH_REQUIRE(decrypted.regularTxsMap);
    CATCH_REQUIRE(decrypted.regularTxsMap->size() == 1);
    CATCH_REQUIRE(decrypted.regularTxsMap->count(1) == 1);
}

CATCH_TEST_CASE("BiteEngine decrypts treats out of place CAT as regular tx", "[bite][engine][decrypt][cat][mixed]") {
    const uint64_t epoch = 11;
    BiteCore core;
    core.doRealCrypto = true;
    BiteEngine engine(core, BiteConfig{});
    auto keys = generateKeys(1, 1);

    // 1 BITE1
    auto biteTx = buildBite1Transaction(
        {0xAA},
        std::vector<uint8_t>(ADDRESS_SIZE, 0x44),
        epoch,
        keys.commonPublic
    );

    // 1 cat at end (out of place)
    auto catTx = buildBite2Transaction(
        { {0x01}, {0x02} },
        { {0x10}, {0x20} },
        epoch,
        keys.commonPublic
    );

    auto txVec = std::make_shared<std::vector<ptr<Transaction>>>();
    txVec->push_back(biteTx); // idx 0
    txVec->push_back(catTx);  // idx 1
    TransactionList txList(txVec);

    DecryptedAESKeyList aesKeys;
    aesKeys.addKeys(0, getDecryptedAESKeysForTransaction(biteTx, keys, 1, 1, epoch));

    BiteRuntimeContext ctx{epoch, std::make_shared<folly::CPUThreadPoolExecutor>(2)
                            , true
                          };
    auto decrypted = engine.decryptTransactionsListInParallel(txList, aesKeys, ctx);

    // no cat decrypted
    CATCH_REQUIRE(decrypted.ctxTxsMap);
    CATCH_REQUIRE(decrypted.ctxTxsMap->empty());
    // 1 regular decrypted
    CATCH_REQUIRE(decrypted.regularTxsMap);
    CATCH_REQUIRE(decrypted.regularTxsMap->size() == 1);
    CATCH_REQUIRE(decrypted.regularTxsMap->count(0) == 1); // tx 0 decrypted
    CATCH_REQUIRE(decrypted.regularTxsMap->count(1) == 0); // tx 1 not decrypted - regular
}

CATCH_TEST_CASE("BiteEngine decrypt regular tx missing AES keys throws", "[bite][engine][decrypt][error]") {
    const uint64_t epoch = 14;
    BiteCore core;
    core.doRealCrypto = true;
    BiteEngine engine(core, BiteConfig{});
    auto keys = generateKeys(1, 1);

    auto biteTx = buildBite1Transaction(
        {0x01},
        std::vector<uint8_t>(ADDRESS_SIZE, 0x22),
        epoch,
        keys.commonPublic
    );

    auto txVec = std::make_shared<std::vector<ptr<Transaction>>>();
    txVec->push_back(biteTx);
    TransactionList txList(txVec);

    DecryptedAESKeyList aesKeys; // no keys

    BiteRuntimeContext ctx{epoch, std::make_shared<folly::CPUThreadPoolExecutor>(2)
                            , true
                          };
    CATCH_REQUIRE_THROWS(engine.decryptTransactionsListInParallel(txList, aesKeys, ctx));
}

CATCH_TEST_CASE("BiteEngine decrypt throws on CAT AES key count mismatch", "[bite][engine][decrypt][error][cat]") {
    const uint64_t epoch = 13;
    BiteCore core;
    core.doRealCrypto = true;
    BiteEngine engine(core, BiteConfig{});
    auto keys = generateKeys(1, 1);

    auto catTx = buildBite2Transaction(
        { {0x01}, {0x02} },
        { {0x10}, {0x20} },
        epoch,
        keys.commonPublic
    );

    auto txVec = std::make_shared<std::vector<ptr<Transaction>>>();
    txVec->push_back(catTx);
    TransactionList txList(txVec);

    DecryptedAESKeyList aesKeys;
    // CAT tx has 2 ciphertexts; provide only 1 key to force count mismatch
    aesKeys.addKeys(0, makeDecryptedKeys(1, 0x01));

    BiteRuntimeContext ctx{epoch, std::make_shared<folly::CPUThreadPoolExecutor>(2), true};
    CATCH_REQUIRE_THROWS(engine.decryptTransactionsListInParallel(txList, aesKeys, ctx));
}

CATCH_TEST_CASE("BiteEngine mergeAESKeys combines shares into AES keys (mock)", "[bite][engine][merge]") {
    const block_id blockId = 21;
    const size_t required = 2;
    const size_t total = 3;
    BiteEngine engine = makeEngine(true, BiteConfig{required, total});
    auto keySet = generateKeys(required, total);

    TransactionCiphertextsMap txCiphertexts;
    auto c1 = makeValidCiphertextWithKey(5, keySet.commonPublic);
    auto c2 = makeValidCiphertextWithKey(5, keySet.commonPublic);
    txCiphertexts.emplace(0, std::make_shared<TransactionCiphertexts>(c1));
    txCiphertexts.emplace(1, std::make_shared<TransactionCiphertexts>(c2));

    std::map<schain_index, std::shared_ptr<AESKeyDecryptionShareList>> shareMap;
    shareMap.emplace(1, makeConsensusShareList(blockId, 1, txCiphertexts, keySet));
    shareMap.emplace(2, makeConsensusShareList(blockId, 2, txCiphertexts, keySet));

    BiteRuntimeContext ctx{5, nullptr};
    auto aesKeys = engine.mergeAESKeys(
        blockId,
        txCiphertexts,
        shareMap,
        keySet.publicKeys,
        ctx
    );

    CATCH_REQUIRE(aesKeys);
    CATCH_REQUIRE(aesKeys->getSize() == 2);
    CATCH_REQUIRE(aesKeys->totalDecryptedCiphertextsCount() == 2);
    CATCH_REQUIRE(aesKeys->getKeys(0));
    CATCH_REQUIRE(aesKeys->getKeys(1));
}

CATCH_TEST_CASE("BiteEngine mergeAESKeys throws if not enough share sets", "[bite][engine][merge][error]") {
    const block_id blockId = 22;
    const size_t required = 2;
    const size_t total = 3;
    BiteEngine engine = makeEngine(true, BiteConfig{required, total});
    auto keySet = generateKeys(required, total);

    TransactionCiphertextsMap txCiphertexts;
    auto c1 = makeValidCiphertextWithKey(5, keySet.commonPublic);
    txCiphertexts.emplace(0, std::make_shared<TransactionCiphertexts>(c1));

    std::map<schain_index, std::shared_ptr<AESKeyDecryptionShareList>> shareMap;
    shareMap.emplace(1, makeConsensusShareList(blockId, 1, txCiphertexts, keySet)); // only 1, but requiredSigners=2

    BiteRuntimeContext ctx{5, nullptr};
    CATCH_REQUIRE_THROWS(engine.mergeAESKeys(
        blockId,
        txCiphertexts,
        shareMap,
        keySet.publicKeys,
        ctx
    ));
}

CATCH_TEST_CASE("BiteEngine mergeAESKeys handles CAT ciphertexts", "[bite][engine][merge][cat]") {
    const block_id blockId = 23;
    const size_t required = 2;
    const size_t total = 3;
    const uint64_t epoch = 7;
    BiteEngine engine = makeEngine(true, BiteConfig{required, total});
    auto keySet = generateKeys(required, total);

    TransactionCiphertextsMap txCiphertexts;
    auto catCiphertexts = makeCatCiphertextsWithKey(epoch, keySet.commonPublic, 2);
    txCiphertexts.emplace(0, std::make_shared<TransactionCiphertexts>(catCiphertexts));

    std::map<schain_index, std::shared_ptr<AESKeyDecryptionShareList>> shareMap;
    shareMap.emplace(1, makeConsensusShareList(blockId, 1, txCiphertexts, keySet));
    shareMap.emplace(2, makeConsensusShareList(blockId, 2, txCiphertexts, keySet));

    BiteRuntimeContext ctx{epoch, nullptr};
    auto aesKeys = engine.mergeAESKeys(
        blockId,
        txCiphertexts,
        shareMap,
        keySet.publicKeys,
        ctx
    );

    CATCH_REQUIRE(aesKeys);
    CATCH_REQUIRE(aesKeys->getSize() == 1);
    CATCH_REQUIRE(aesKeys->totalDecryptedCiphertextsCount() == 2);
    CATCH_REQUIRE(aesKeys->getKeys(0));
}

CATCH_TEST_CASE("BiteEngine mergeAESKeys handles mixed BITE1 and CAT ciphertexts", "[bite][engine][merge][mixed]") {
    const block_id blockId = 24;
    const size_t required = 2;
    const size_t total = 3;
    const uint64_t epoch = 8;
    BiteEngine engine = makeEngine(true, BiteConfig{required, total});
    auto keySet = generateKeys(required, total);

    TransactionCiphertextsMap txCiphertexts;
    auto regular = makeValidCiphertextWithKey(epoch, keySet.commonPublic);
    txCiphertexts.emplace(0, std::make_shared<TransactionCiphertexts>(regular));
    auto catCiphertexts = makeCatCiphertextsWithKey(epoch, keySet.commonPublic, 2);
    txCiphertexts.emplace(1, std::make_shared<TransactionCiphertexts>(catCiphertexts));

    std::map<schain_index, std::shared_ptr<AESKeyDecryptionShareList>> shareMap;
    shareMap.emplace(1, makeConsensusShareList(blockId, 1, txCiphertexts, keySet));
    shareMap.emplace(2, makeConsensusShareList(blockId, 2, txCiphertexts, keySet));

    BiteRuntimeContext ctx{epoch, nullptr};
    auto aesKeys = engine.mergeAESKeys(
        blockId,
        txCiphertexts,
        shareMap,
        keySet.publicKeys,
        ctx
    );

    CATCH_REQUIRE(aesKeys);
    CATCH_REQUIRE(aesKeys->getSize() == 2);
    CATCH_REQUIRE(aesKeys->totalDecryptedCiphertextsCount() == 3);
    CATCH_REQUIRE(aesKeys->getKeys(0));
    CATCH_REQUIRE(aesKeys->getKeys(1));
}

// ============ Empty ciphertext CAT tests ============ //

CATCH_TEST_CASE("BiteEngine validateCiphertexts handles CAT with empty ciphertexts", "[bite][engine][validate][cat][empty]") {
    BiteEngine engine = makeEngine(true);
    TransactionCiphertextsMap map;

    // tx 0 - CAT with valid ciphertexts
    auto c1 = makeValidCiphertext(5);
    auto c2 = makeValidCiphertext(5);
    std::vector<ptr<BiteCiphertext>> tx0Ciphertexts{c1, c2};
    map.emplace(0, std::make_shared<TransactionCiphertexts>(tx0Ciphertexts));

    // tx 1 - CAT with empty ciphertexts (0 ciphertexts)
    std::vector<ptr<BiteCiphertext>> emptyCiphertexts;
    map.emplace(1, std::make_shared<TransactionCiphertexts>(emptyCiphertexts));

    // tx 2 - another valid CAT
    auto c3 = makeValidCiphertext(5);
    map.emplace(2, std::make_shared<TransactionCiphertexts>(c3));

    BiteRuntimeContext runtimeContext;
    auto result = engine.validateCiphertexts(map, runtimeContext);
    CATCH_REQUIRE(result.allValid());
    CATCH_REQUIRE(result.invalidCiphertextIndices.empty());
    // public values only for non-empty ciphertexts (tx0 has 2, tx1 has 0, tx2 has 1 = 3 total)
    CATCH_REQUIRE(result.publicDecryptionValues.size() == 3);
}

CATCH_TEST_CASE("BiteEngine decrypts CAT with empty ciphertexts at start of list", "[bite][engine][decrypt][cat][empty]") {
    const uint64_t epoch = 15;
    BiteCore core;
    core.doRealCrypto = true;
    BiteEngine engine(core, BiteConfig{});
    auto keys = generateKeys(1, 1);

    // CAT 0 - empty ciphertexts (nothing encrypted, just plain args)
    auto emptyCat = buildBite2Transaction(
        {},  // no encrypted args
        { {0x10}, {0x20} },  // only plain args
        epoch,
        keys.commonPublic
    );

    // CAT 1 - valid CAT with 2 encrypted args
    auto validCat = buildBite2Transaction(
        { {0x01}, {0x02} },
        { {0xAA} },
        epoch,
        keys.commonPublic
    );

    auto txVec = std::make_shared<std::vector<ptr<Transaction>>>();
    txVec->push_back(emptyCat);  // idx 0
    txVec->push_back(validCat);  // idx 1
    TransactionList txList(txVec);

    DecryptedAESKeyList aesKeys;
    // No keys for tx 0 (empty ciphertexts)
    aesKeys.addKeys(1, getDecryptedAESKeysForTransaction(validCat, keys, 1, 1, epoch));

    BiteRuntimeContext ctx{epoch, std::make_shared<folly::CPUThreadPoolExecutor>(2), true};
    auto decrypted = engine.decryptTransactionsListInParallel(txList, aesKeys, ctx);

    CATCH_REQUIRE(decrypted.ctxTxsMap);
    CATCH_REQUIRE(decrypted.ctxTxsMap->size() == 2);
    // tx 0 should be present with empty args
    auto it0 = decrypted.ctxTxsMap->find(0);
    CATCH_REQUIRE(it0 != decrypted.ctxTxsMap->end());
    CATCH_REQUIRE(it0->second.has_value());
    CATCH_REQUIRE(it0->second->args.empty());
    // tx 1 should have decrypted args
    auto it1 = decrypted.ctxTxsMap->find(1);
    CATCH_REQUIRE(it1 != decrypted.ctxTxsMap->end());
    CATCH_REQUIRE(it1->second.has_value());
    CATCH_REQUIRE(it1->second->args.size() == 2);
}

CATCH_TEST_CASE("BiteEngine decrypts CAT with empty ciphertexts in middle of list", "[bite][engine][decrypt][cat][empty]") {
    const uint64_t epoch = 16;
    BiteCore core;
    core.doRealCrypto = true;
    BiteEngine engine(core, BiteConfig{});
    auto keys = generateKeys(1, 1);

    // CAT 0 - valid CAT
    auto cat0 = buildBite2Transaction(
        { {0x01} },
        { {0xAA} },
        epoch,
        keys.commonPublic
    );

    // CAT 1 - empty ciphertexts
    auto emptyCat = buildBite2Transaction(
        {},  // no encrypted args
        { {0x10}, {0x20}, {0x30} },  // only plain args
        epoch,
        keys.commonPublic
    );

    // CAT 2 - valid CAT
    auto cat2 = buildBite2Transaction(
        { {0x02}, {0x03} },
        { {0xBB} },
        epoch,
        keys.commonPublic
    );

    auto txVec = std::make_shared<std::vector<ptr<Transaction>>>();
    txVec->push_back(cat0);     // idx 0
    txVec->push_back(emptyCat); // idx 1
    txVec->push_back(cat2);     // idx 2
    TransactionList txList(txVec);

    DecryptedAESKeyList aesKeys;
    aesKeys.addKeys(0, getDecryptedAESKeysForTransaction(cat0, keys, 1, 1, epoch));
    // No keys for tx 1 (empty ciphertexts)
    aesKeys.addKeys(2, getDecryptedAESKeysForTransaction(cat2, keys, 1, 1, epoch));

    BiteRuntimeContext ctx{epoch, std::make_shared<folly::CPUThreadPoolExecutor>(2), true};
    auto decrypted = engine.decryptTransactionsListInParallel(txList, aesKeys, ctx);

    CATCH_REQUIRE(decrypted.ctxTxsMap);
    CATCH_REQUIRE(decrypted.ctxTxsMap->size() == 3);
    // tx 0 should have 1 decrypted arg
    auto it0 = decrypted.ctxTxsMap->find(0);
    CATCH_REQUIRE(it0 != decrypted.ctxTxsMap->end());
    CATCH_REQUIRE(it0->second.has_value());
    CATCH_REQUIRE(it0->second->args.size() == 1);
    // tx 1 should be present with empty args
    auto it1 = decrypted.ctxTxsMap->find(1);
    CATCH_REQUIRE(it1 != decrypted.ctxTxsMap->end());
    CATCH_REQUIRE(it1->second.has_value());
    CATCH_REQUIRE(it1->second->args.empty());
    // tx 2 should have 2 decrypted args
    auto it2 = decrypted.ctxTxsMap->find(2);
    CATCH_REQUIRE(it2 != decrypted.ctxTxsMap->end());
    CATCH_REQUIRE(it2->second.has_value());
    CATCH_REQUIRE(it2->second->args.size() == 2);
}

CATCH_TEST_CASE("BiteEngine decrypts CAT with empty ciphertexts at end of list", "[bite][engine][decrypt][cat][empty]") {
    const uint64_t epoch = 17;
    BiteCore core;
    core.doRealCrypto = true;
    BiteEngine engine(core, BiteConfig{});
    auto keys = generateKeys(1, 1);

    // CAT 0 - valid CAT
    auto cat0 = buildBite2Transaction(
        { {0x01}, {0x02} },
        { {0xAA} },
        epoch,
        keys.commonPublic
    );

    // CAT 1 - valid CAT
    auto cat1 = buildBite2Transaction(
        { {0x03} },
        { {0xBB}, {0xCC} },
        epoch,
        keys.commonPublic
    );

    // CAT 2 - empty ciphertexts at end
    auto emptyCat = buildBite2Transaction(
        {},  // no encrypted args
        { {0x10} },  // only plain args
        epoch,
        keys.commonPublic
    );

    auto txVec = std::make_shared<std::vector<ptr<Transaction>>>();
    txVec->push_back(cat0);     // idx 0
    txVec->push_back(cat1);     // idx 1
    txVec->push_back(emptyCat); // idx 2
    TransactionList txList(txVec);

    DecryptedAESKeyList aesKeys;
    aesKeys.addKeys(0, getDecryptedAESKeysForTransaction(cat0, keys, 1, 1, epoch));
    aesKeys.addKeys(1, getDecryptedAESKeysForTransaction(cat1, keys, 1, 1, epoch));
    // No keys for tx 2 (empty ciphertexts)

    BiteRuntimeContext ctx{epoch, std::make_shared<folly::CPUThreadPoolExecutor>(2), true};
    auto decrypted = engine.decryptTransactionsListInParallel(txList, aesKeys, ctx);

    CATCH_REQUIRE(decrypted.ctxTxsMap);
    CATCH_REQUIRE(decrypted.ctxTxsMap->size() == 3);
    // tx 0 should have 2 decrypted args
    auto it0 = decrypted.ctxTxsMap->find(0);
    CATCH_REQUIRE(it0 != decrypted.ctxTxsMap->end());
    CATCH_REQUIRE(it0->second.has_value());
    CATCH_REQUIRE(it0->second->args.size() == 2);
    // tx 1 should have 1 decrypted arg
    auto it1 = decrypted.ctxTxsMap->find(1);
    CATCH_REQUIRE(it1 != decrypted.ctxTxsMap->end());
    CATCH_REQUIRE(it1->second.has_value());
    CATCH_REQUIRE(it1->second->args.size() == 1);
    // tx 2 should be present with empty args
    auto it2 = decrypted.ctxTxsMap->find(2);
    CATCH_REQUIRE(it2 != decrypted.ctxTxsMap->end());
    CATCH_REQUIRE(it2->second.has_value());
    CATCH_REQUIRE(it2->second->args.empty());
}

CATCH_TEST_CASE("BiteEngine decrypts only CAT with empty ciphertexts", "[bite][engine][decrypt][cat][empty]") {
    const uint64_t epoch = 18;
    BiteCore core;
    core.doRealCrypto = true;
    BiteEngine engine(core, BiteConfig{});
    auto keys = generateKeys(1, 1);

    // Single CAT with no encrypted args
    auto emptyCat = buildBite2Transaction(
        {},  // no encrypted args
        { {0xAA}, {0xBB} },  // only plain args
        epoch,
        keys.commonPublic
    );

    auto txVec = std::make_shared<std::vector<ptr<Transaction>>>();
    txVec->push_back(emptyCat);
    TransactionList txList(txVec);

    DecryptedAESKeyList aesKeys; // no keys needed

    BiteRuntimeContext ctx{epoch, std::make_shared<folly::CPUThreadPoolExecutor>(2), true};
    auto decrypted = engine.decryptTransactionsListInParallel(txList, aesKeys, ctx);

    CATCH_REQUIRE(decrypted.ctxTxsMap);
    CATCH_REQUIRE(decrypted.ctxTxsMap->size() == 1);
    auto it = decrypted.ctxTxsMap->find(0);
    CATCH_REQUIRE(it != decrypted.ctxTxsMap->end());
    CATCH_REQUIRE(it->second.has_value());
    CATCH_REQUIRE(it->second->args.empty());
}

CATCH_TEST_CASE("BiteEngine tryGetEncryptedCTXArgs caches SC address as AAD", "[bite][engine][cat][aad]") {
    const uint64_t epoch = 20;
    auto keys = generateKeys(1, 1);
    
    // Build CAT transaction with specific SC address
    std::vector<uint8_t> scAddress(ADDRESS_SIZE, 0x42);
    
    auto catTx = buildBite2TransactionWithScAddress(
        { {0x01, 0x02} },
        { {0xAA} },
        epoch,
        keys.commonPublic,
        scAddress
    );
    
    // First call should parse and cache
    auto args = BiteEngine::tryGetEncryptedCTXArgs(catTx, epoch);
    CATCH_REQUIRE(args != nullptr);
    CATCH_REQUIRE(!args->empty());
    
    // Verify SC address was cached in transaction
    auto cachedScAddr = catTx->getScAddressAadTE();
    CATCH_REQUIRE(cachedScAddr != nullptr);
    CATCH_REQUIRE(std::vector<uint8_t>(cachedScAddr->begin(), cachedScAddr->end()) == scAddress);
}

// Performance Tests

CATCH_TEST_CASE("BiteEngine mergeAESKeys performance", "[bite][engine][merge][performance]") {
    const block_id blockId = 30;

    // 10 node threshold out of 15
    const size_t required = 3;
    const size_t total = 4;

    // total BITE txs
    const uint32_t numTxs = 250;
    const uint32_t numCATTxs = 100;

    const uint64_t epoch = 10;

    BiteEngine engine = makeEngine(true, BiteConfig{required, total});
    auto keySet = generateKeys(required, total);

    // build ciphertexts of timer
    TransactionCiphertextsMap txCiphertexts;
    for (size_t i = 0; i < numCATTxs; ++i) {
        auto c = makeCatCiphertextsWithKey(epoch, keySet.commonPublic, 2); // CAT with 2 ciphertexts each
        txCiphertexts.emplace(i, std::make_shared<TransactionCiphertexts>(c));
    }

    for (size_t i = numCATTxs; i < numCATTxs + numTxs; ++i) {
        auto c = makeValidCiphertextWithKey(epoch, keySet.commonPublic);
        txCiphertexts.emplace(i, std::make_shared<TransactionCiphertexts>(c));
    }

    // compute decryption shares off timer
    std::map<schain_index, std::shared_ptr<AESKeyDecryptionShareList>> shareMap;
    for (size_t i = 1; i <= total; ++i) {
        shareMap.emplace(i, makeConsensusShareList(blockId, i, txCiphertexts, keySet));
    }

    BiteRuntimeContext ctx{
        epoch,
        std::make_shared<folly::CPUThreadPoolExecutor>(20), // use thread pool for merging in performance test 
    };
    
    auto start = std::chrono::high_resolution_clock::now();
    auto aesKeys = engine.mergeAESKeys(
        blockId,
        txCiphertexts,
        shareMap,
        keySet.publicKeys,
        ctx
    );
    auto end = std::chrono::high_resolution_clock::now();
    
    std::chrono::duration<double> duration = end - start;
    std::cout << "mergeAESKeys took " << duration.count() << " seconds for " << numTxs 
              << " BITE transactions + " << numCATTxs << " CAT transactions" << std::endl;

    CATCH_REQUIRE(aesKeys);
}

#endif // BITE
