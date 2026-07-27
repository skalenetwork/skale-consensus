#pragma once
#include "SkaleCommon.h"
#include "bite/BiteCore.h"
#include "bite/crypto/CryptographicValidationMode.h"
#include "crypto/AESKeyDecryptionShare.h"
#include "datastructures/TransactionCiphertextsMap.h"

#include <memory>
#include <map>

class TransactionCiphertextsMap;
class TransactionList;
class Transaction;
class DecryptedAESKeyList;
class AESKeyDecryptionShareSet;
class AESKeyDecryptionShareList;

namespace libBLS {
    class TEPublicKeyShare;
}


struct BiteConfig {
    size_t requiredSigners{0};
    size_t totalSigners{0};
    bool sgxEnabled{true};
};

struct BiteRuntimeContext {
    epoch_id currentEpoch{0};
    std::shared_ptr<folly::CPUThreadPoolExecutor> threadPoolExecutor{nullptr};
    bool isBite2PatchEnabled{false};
};

class BiteEngine {
private:
    BiteCore  core;
    BiteConfig config;

public:
    BiteEngine(BiteCore _core, BiteConfig _config)
        : core(_core), config(_config) {}

    bool usingRealCrypto() const {
        return core.doRealCrypto;
    }

    //=================== Stage 1: Parsing BITE transactions =================== //
    struct ParseResult {
        TransactionCiphertextsMap txsCiphertexts;

        // Indices of transactions that failed to parse
        // txs can fail if they have the correct tag (BITE1 address or BITE2 function signature)
        // but invalid format
        std::vector<transaction_index> failedTransactions;
    };

    /**
     * @brief Stage 1: Parse BITE transactions from the transaction list.
     * Attempts to parse all BITE transactions in the given TransactionList.
     * Caches parsed BiteCiphertext objects in the Transaction objects.
     * Assumes CTXs are at the start of the list, followed by regular transactions.
     */
    static ParseResult parseAndCacheBITETransactions(
        const TransactionList& txList,
        BiteRuntimeContext& runtimeContext
    );

    //=================== Stage 2: Ciphertext Validation  =================== //
    
    struct CiphertextValidationResult {
        // Indices of transactions with invalid ciphertexts
        std::vector<transaction_index> invalidCiphertextIndices;

        // reasons for failures, aligned with invalidCiphertextIndices
        std::vector<std::string> failureReasons;

        // Public decryption values for valid ciphertexts - only if all are valid
        std::vector<std::string> publicDecryptionValues;

        bool allValid() const noexcept {
            return invalidCiphertextIndices.empty();
        }
    };

    /**
     * @brief Validates the ciphertexts extracted from BITE transactions.
     * Failures can either be from parsing from bytes or semantic validation.
     * Each transaction can have multiple ciphertexts (e.g., CTX transactions).
     * Thus, if a single ciphertext in a transaction is invalid, the whole transaction is marked invalid.
     * Only if all ciphertexts are valid, public decryption values are returned.
     */
    CiphertextValidationResult validateCiphertexts(
        const TransactionCiphertextsMap& txsCiphertexts,
        const BiteRuntimeContext& runtimeContext
    ) const;

    //=================== Stage 3: Merging TE Decryption Shares into AES Keys  =================== //

    std::shared_ptr<DecryptedAESKeyList> mergeAESKeys(
        block_id _blockId,
        TransactionCiphertextsMap& _txCiphertexts,
        const std::map<schain_index, std::shared_ptr<AESKeyDecryptionShareList>>& _decryptionShareMap,
        const std::vector<libBLS::TEPublicKeyShare>& _tePublicKeyShares,
        const BiteRuntimeContext& _runtimeContext
    ) const;

    //=================== Stage 4: Decrypt transactions from decrypted keys  =================== //

    DecryptedTransactions decryptTransactionsListInParallel(
            const TransactionList &_transactionList,
            const DecryptedAESKeyList &_aesKeys,
            BiteRuntimeContext& runtimeContext
    ) const;

    //=================== Helpers =================== //

    /**
     * @brief Tries to parse the encrypted regular transaction fields from the given Transaction.
     * Caches the parsed BiteCiphertext in the Transaction object if successful.
     */
    static ptr<BiteCiphertext> tryGetEncryptedRegularTxFields(
                const ptr<Transaction> &_transaction, epoch_id _currentEpochId);

    std::vector<uint8_t> buildRegularTxData(
        const libBLS::TEPublicKey& key,
        const std::vector<uint8_t>& plainData,
        const std::vector<uint8_t>& to,
        uint64_t epochId
    ) const;


    static ptr<std::vector<ptr<BiteCiphertext>>> tryGetEncryptedCTXArgs(
            const ptr<Transaction>& _transaction, epoch_id _currentEpochId );

    std::vector<uint8_t> buildCTXData(
        const libBLS::TEPublicKey& key,
        size_t numberOfCiphertexts,
        uint64_t epochId,
        const std::optional<AddressBytes>& scAddressAadTE = std::nullopt
    ) const;


    /**
     * @brief Creates AESKeyDecryptionShares objects from their string representations.
     * If using real crypto, optionally validates the shares during creation. 
     * If validation fails, an exception is thrown.
     * If using mockup, no validation.
     * @param shareStrs vector of string representations of decryption shares, each corresponding to a ciphertext
     * @param decryptorIndex the index of the decryptor that created these shares
     * @param decryptionFailed whether the decryption shares correspond to a failed decryption
     * @param validate whether to validate the shares during creation (only applicable if using real crypto)
     */
    ptr<AESKeyDecryptionShares> createDecryptionSharesObjects(
        const std::vector<std::string_view>& shareStrs,
        schain_index decryptorIndex,
        bool decryptionFailed,
        CryptographicValidationMode validationMode = CryptographicValidationMode::Validate
    ) const;

    ptr<AESKeyDecryptionShareSet> createAESDecryptionShareSetObject(
        block_id _blockId, transaction_index _transactionIndex, size_t numberOfCiphertexts) const;

    /**
     * @brief Unflattens a list of decryption shares into a vector of AESKeyDecryptionShares,
     * each corresponding to a ciphertext in the given TransactionCiphertextsMap.
     * Follows the order of ciphertexts in the map.
     */
    ptr<vector<ptr<AESKeyDecryptionShares>>> unflattenDecryptionShares(
        const TransactionCiphertextsMap& _txsCiphertexts,
        const ptr<vector<ptr<AESKeyDecryptionShare>>> _allSharesFlattened,
        schain_index _decryptorIndex
    ) const;

};
