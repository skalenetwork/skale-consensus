#pragma once

#include <memory>
#include <vector>
#include "bite/BiteCiphertext.h"
#include "SkaleCommon.h"
#include "datastructures/TransactionCiphertextsMap.h"
#include "bite/BiteCore.h"

/**
 * @brief Codec for BITE transactions - parsing from and building to Transaction fields.
 * Holds all functionality related to BITE transaction encoding/decoding, encryption, and decryption.
 * Represents a detached module that does not depend on other parts of the system (such as BiteManager, Consensus, etc).
 */
struct BiteCodec {

    // ==================== BiteCiphertext parsing  from Transaction fields ==================== //

    static std::shared_ptr<BiteCiphertext> tryParseEncryptedRegularTxFields(
        std::vector<uint8_t>& _to, ptr<std::vector<uint8_t>> _data, epoch_id _currentEpochId);

    /**
     * Parses BITE2 CTX calldata:
     *
     *   function selector (4 bytes) ||
     *   RLP([
     *       RLP([encrypted ciphertext 1, encrypted ciphertext 2, ...]),
     *       RLP([plaintext argument 1, plaintext argument 2, ...])
     *   ])
     *
     * Each encrypted ciphertext is a byte string containing serialized epoched BITE data.
     * Returns nullptr when the selector does not identify a CTX call; throws when matching
     * selector data is malformed.
     */
    static std::shared_ptr<std::vector<std::shared_ptr<BiteCiphertext>>> tryParseEncryptedCTXArgs(
        const std::vector<uint8_t>& _dataField, epoch_id _currentEpochId);

    // ======================== Parsing decrypted data into tx fields ======================== //

    /**
     * @brief Tries to convert decrypted data into DecryptedRegularTxFields structure.
     * This function will try to extract a 'to' address and 'data' fields from the decrypted byte array.
     */
    static DecryptedRegularTxFields parseRegularTxDecryptedData(
        const vector<uint8_t> &_data);

    // ==================== BiteCiphertext building for Transaction fields ==================== //
    
    static std::vector<uint8_t> encodeCTXData(
        const std::vector<std::vector<uint8_t>>& encryptedSerializedArgs,
        const std::vector<std::vector<uint8_t>>& plainArgs);

    static std::vector<uint8_t> encodeRegularTxPayload(
        const std::vector<uint8_t>& _plainData, const std::vector<uint8_t>& _to);


    static vector<uint8_t> decryptCiphertext(const BiteCiphertext&_bite, 
        const libBLS::AES256Key& _decryptedAESKey, const BiteCore& core);

    

    // ==================== Bite epoched data ==================== //

    struct EpochedBiteData {
        uint64_t epochId;
        std::shared_ptr<std::vector<uint8_t>> keyPlusEncryptedData;
    };
    
    /**
     * @brief Builds serialized BITE data with epoch prefix, in RLP format.
     */
    static std::vector<uint8_t> encodeEpochedBiteData(
        const std::vector<uint8_t>& _keyPlusEncryptedData,
        uint64_t _epoch
    );

    /**
     * @brief Decodes serialized BITE data with epoch prefix, in RLP format.
     * Doesn't do any epoch validation against the keys. Performs only deserialization.
     */
    static EpochedBiteData decodeEpochedBiteData(
        const std::vector<uint8_t>& _serializedData
    );

    

    // ==================== Share serialization into/from string ==================== //

    static std::vector<std::string_view> splitShares(std::string_view s);

};
