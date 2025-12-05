#pragma once

#ifdef BITE

#include <memory>
#include <mutex>
#include <vector>
#include <boost/endian/conversion.hpp>

#include "bite/BiteCodec.h"
#include "bite/BiteCore.h"
#include "bite/Constants.h"
#include "datastructures/Transaction.h"
#include "libBLS/threshold_encryption/TEPublicKey.h"
#include "libBLS/threshold_encryption/ThresholdEncryption.h"
#include "libBLS/threshold_encryption/threshold_encryption.h"
#include "rlp/EthTransactionEncoder.h"

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

#ifdef BITE2
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
    auto dataField = BiteCodec::encodeCATData(serializedEncryptedArgs, plainArgs);

    // generate sample tx, and set BITE2 data field
    auto tx = EthTransactionEncoder::generateSampleTx();
    tx->to = std::vector<uint8_t>(BITE_ADDRESS_AS_BYTE_ARRAY, BITE_ADDRESS_AS_BYTE_ARRAY + ADDRESS_SIZE);
    tx->data = dataField;

    // encode and return transaction
    auto encoded = EthTransactionEncoder::signAndEncodeTx(tx);
    return std::make_shared<Transaction>(encoded, false);
}
#endif

}  // namespace BiteTestUtils

#endif
