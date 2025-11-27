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
        std::vector<uint8_t>& _to, std::vector<uint8_t>& _data, epoch_id _currentEpochId);

    static std::shared_ptr<std::vector<std::shared_ptr<BiteCiphertext>>> tryParseEncryptedCATArgs(
        const std::vector<uint8_t>& _dataField, epoch_id _currentEpochId);




    // ======================== Parsing decrypted data into tx fields ======================== //

    /**
     * @brief Tries to convert decrypted data into DecryptedRegularTxFields structure.
     * This function will try to extract a 'to' address and 'data' fields from the decrypted byte array.
     */
    static DecryptedRegularTxFields parseRegularTxDecryptedData(
        const vector<uint8_t> &_data);

    // ==================== BiteCiphertext building for Transaction fields ==================== //
    

    static std::vector<uint8_t> encodeCATData(
        const std::vector<std::vector<uint8_t>>& encryptedSerializedArgs,
        const std::vector<std::vector<uint8_t>>& plainArgs);

    static std::vector<uint8_t> encodeRegularTxPayload(
        const std::vector<uint8_t>& _plainData, const std::vector<uint8_t>& _to);


    static vector<uint8_t> decryptCiphertext(const BiteCiphertext&_bite, 
        const libBLS::AES256Key& _decryptedAESKey, const BiteCore& core);

};