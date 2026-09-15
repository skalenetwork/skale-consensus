#pragma once

#ifdef BITE

#include <ctime>
#include <memory>
#include <mutex>
#include <vector>
#include <boost/endian/conversion.hpp>

#include "bite/BiteCodec.h"
#include "bite/BiteCore.h"
#include "bite/BiteManager.h"
#include "bite/Constants.h"
#include "chains/Schain.h"
#include "crypto/CryptoManager.h"
#include "datastructures/Transaction.h"
#include "libBLS/threshold_encryption/TEPublicKey.h"
#include "libBLS/threshold_encryption/ThresholdEncryption.h"
#include "libBLS/threshold_encryption/threshold_encryption.h"
#include "node/ConsensusEngine.h"
#include "node/Node.h"
#include "node/NodeInfo.h"
#include "crypto/AESKeyDecryptionShareList.h"
#include "crypto/MockupAESKeyDecryptionShare.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/MyBlockProposal.h"
#include "datastructures/TransactionCiphertextsMap.h"
#include "datastructures/TransactionList.h"
#include "libBLS/test/utils.h"
#include "rlp/EthTransactionEncoder.h"
#include "thirdparty/json.hpp"
#include "tests/TestUtils.h"

namespace BiteTestUtils {

inline void ensureLibBLSInitialized() {
    static std::once_flag initFlag;
    std::call_once(initFlag, []() { libBLS::init(); });
}

inline std::vector<uint8_t> buildBITE1EncryptedData(
    const std::vector<uint8_t>& plainData,
    const std::vector<uint8_t>& toAddress,
    const libBLS::TEPublicKey& tePublicKey) {
    
    auto payload = BiteCodec::encodeRegularTxPayload(plainData, toAddress);

    auto ciphertext = libBLS::ThresholdEncryption::encrypt(payload, tePublicKey);
    return ciphertext.toBytes();
}

inline std::vector<uint8_t> buildBITE1EpochedData(
    const std::vector<uint8_t>& plainData,
    const std::vector<uint8_t>& toAddress,
    uint64_t epoch,
    const libBLS::TEPublicKey& tePublicKey) {

    auto encryptedData = buildBITE1EncryptedData(plainData, toAddress, tePublicKey);
    return BiteCodec::encodeEpochedBiteData(
        encryptedData, epoch
    );
}

inline std::shared_ptr<Transaction> buildBite1Transaction(
    const std::vector<uint8_t>& plainData,
    const std::vector<uint8_t>& toAddress,
    uint64_t epoch,
    const libBLS::TEPublicKey& tePublicKey) {

    auto epochedData = buildBITE1EpochedData(
        plainData, toAddress, epoch, tePublicKey
    );

    auto tx = EthTransactionEncoder::generateSampleTx();
    tx->to = std::vector<uint8_t>(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE);
    tx->data = epochedData;
    auto encoded = EthTransactionEncoder::signAndEncodeTx(tx);
    return std::make_shared<Transaction>(encoded, false);
}

inline std::shared_ptr<Transaction> buildBite2Transaction(
    const std::vector<std::vector<uint8_t>>& encryptedArgsPlaintext,
    const std::vector<std::vector<uint8_t>>& plainArgs,
    uint64_t epoch,
    const libBLS::TEPublicKey& tePublicKey,
    bool useRealCrypto = false) {
    ensureLibBLSInitialized();

    BiteCore core;
    core.doRealCrypto = useRealCrypto;

    // encrypt all args
    std::vector<std::vector<uint8_t>> serializedEncryptedArgs;
    serializedEncryptedArgs.reserve(encryptedArgsPlaintext.size());

    for (const auto& arg : encryptedArgsPlaintext) {
        auto ciphertext = libBLS::ThresholdEncryption::encrypt(arg, tePublicKey);
        auto epochedData = BiteCodec::encodeEpochedBiteData(
            ciphertext.toBytes(), epoch
        );
        serializedEncryptedArgs.emplace_back(epochedData);
    }

    // build CAT data field
    auto dataField = BiteCodec::encodeCTXData(serializedEncryptedArgs, plainArgs);

    // generate sample tx, and set BITE2 data field
    auto tx = EthTransactionEncoder::generateSampleTx();
    tx->to = std::vector<uint8_t>(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE);
    tx->data = dataField;

    // encode and return transaction
    auto encoded = EthTransactionEncoder::signAndEncodeTx(tx);
    return std::make_shared<Transaction>(encoded, false);
}

// Variant with custom SC address for AAD testing
inline std::shared_ptr<Transaction> buildBite2TransactionWithScAddress(
    const std::vector<std::vector<uint8_t>>& encryptedArgsPlaintext,
    const std::vector<std::vector<uint8_t>>& plainArgs,
    uint64_t epoch,
    const libBLS::TEPublicKey& tePublicKey,
    const std::vector<uint8_t>& scAddress,
    bool useRealCrypto = false) {
    ensureLibBLSInitialized();

    BiteCore core;
    core.doRealCrypto = useRealCrypto;

    // encrypt all args
    std::vector<std::vector<uint8_t>> serializedEncryptedArgs;
    serializedEncryptedArgs.reserve(encryptedArgsPlaintext.size());

    for (const auto& arg : encryptedArgsPlaintext) {
        auto ciphertext = libBLS::ThresholdEncryption::encrypt(arg, tePublicKey);
        auto epochedData = BiteCodec::encodeEpochedBiteData(
            ciphertext.toBytes(), epoch
        );
        serializedEncryptedArgs.emplace_back(epochedData);
    }

    // build CAT data field
    auto dataField = BiteCodec::encodeCTXData(serializedEncryptedArgs, plainArgs);

    // generate sample tx with custom SC address
    auto tx = EthTransactionEncoder::generateSampleTx();
    tx->to = scAddress;
    tx->data = dataField;

    // encode and return transaction
    auto encoded = EthTransactionEncoder::signAndEncodeTx(tx);
    return std::make_shared<Transaction>(encoded, false);
}

// Helper to create a valid CryptoManager with necessary dependencies for tests
inline std::shared_ptr< CryptoManager > createTestCryptoManager(
    std::shared_ptr< Schain >& chain_out, std::shared_ptr< Node >& node_out,
    ConsensusEngine& engine ) {
    TestUtils::createTestNodeAndSchain(node_out, chain_out, engine);
    return std::make_shared< CryptoManager >( *chain_out );
}

// Helper to create a BiteManager for tests
inline std::shared_ptr< BiteManager > createTestBiteManager(
    std::shared_ptr< Schain >& chain ) {
    return std::make_shared< BiteManager >( *chain );
}

// Create a single-transaction BITE1 block proposal for testing.
// The caller supplies the TE keypair; use generateKeys(1,1) for tests that do not
// exercise decryption and generateKeys(t,n) when the fixture requires specific shares.
inline ptr<BlockProposal> makeTestProposal(
    const std::shared_ptr<Schain>& chain,
    const std::shared_ptr<CryptoManager>& cryptoManager,
    block_id blockId,
    const keys& kp) {
    auto epoch = epoch_id(chain->getNode()->getCurrentEpochId());

    auto tx = buildBite1Transaction(
        std::vector<uint8_t>{0x01, 0x02, 0x03},
        std::vector<uint8_t>(20, 0x11),
        static_cast<uint64_t>(epoch),
        kp.commonPublic);

    auto txs = std::make_shared<std::vector<ptr<Transaction>>>();
    txs->push_back(tx);

    const auto timeStamp =
        std::max<uint64_t>(static_cast<uint64_t>(std::time(nullptr)),
                           static_cast<uint64_t>(MODERN_TIME + 1));

    return MyBlockProposal::createMyProposal(
        *chain,
        blockId,
        epoch,
        chain->getSchainIndex(),
        std::make_shared<TransactionList>(txs),
        u256(0x1234),
        timeStamp,
        1,
        cryptoManager);
}

// Build a proposal with zero BITE transactions (an "empty" block) - used to
// exercise the finalization threshold math for empty-ciphertext proposals.
inline ptr<BlockProposal> makeEmptyTestProposal(
    const std::shared_ptr<Schain>& chain,
    const std::shared_ptr<CryptoManager>& cryptoManager,
    block_id blockId) {
    auto epoch = epoch_id(chain->getNode()->getCurrentEpochId());

    auto txs = std::make_shared<std::vector<ptr<Transaction>>>();

    const auto timeStamp =
        std::max<uint64_t>(static_cast<uint64_t>(std::time(nullptr)),
                           static_cast<uint64_t>(MODERN_TIME + 1));

    return MyBlockProposal::createMyProposal(
        *chain,
        blockId,
        epoch,
        chain->getSchainIndex(),
        std::make_shared<TransactionList>(txs),
        u256(0x1234),
        timeStamp,
        1,
        cryptoManager);
}

// Build a mockup AESKeyDecryptionShareList for the given ciphertext map.
// Uses MockupAESKeyDecryptionShare (no real crypto); compatible with the
// mockup merge path when SGX is disabled.
inline ptr<AESKeyDecryptionShareList> makeMockupShareList(
    block_id blockId,
    schain_index proposerIdx,
    schain_index decryptorIdx,
    const TransactionCiphertextsMap& txCiphertexts) {
    auto list = std::make_shared<AESKeyDecryptionShareList>(blockId, proposerIdx, decryptorIdx);
    for (auto&& [txIdx, txCts] : txCiphertexts) {
        auto shares = std::make_shared<AESKeyDecryptionShares>();
        for (auto& encKey : txCts->getCiphertexts()) {
            auto copy = encKey;
            shares->push_back(MockupAESKeyDecryptionShare::mockupDecrypt(copy, decryptorIdx));
        }
        list->addShares(txIdx, shares);
    }
    return list;
}

// Proposal-wrapping overload: extracts block ID, proposer index, and ciphertexts
// from the proposal so call sites need only pass the proposal and decryptor index.
inline ptr<AESKeyDecryptionShareList> makeMockupShareList(
    const ptr<BlockProposal>& proposal,
    schain_index decryptorIdx) {
    return makeMockupShareList(
        proposal->getBlockID(),
        proposal->getProposerIndex(),
        decryptorIdx,
        *proposal->getTransactionCiphertexts());
}

}  // namespace BiteTestUtils

#endif
