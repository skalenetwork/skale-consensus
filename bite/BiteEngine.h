#pragma once
#include "SkaleCommon.h"
#include "bite/BiteCodec.h"
#include "bite/BiteCore.h"

#include <memory>

class TransactionCiphertextsMap;
class TransactionList;
class BiteTxCache;

struct BiteConfig {
    size_t requiredSigners{0};
    size_t totalSigners{0};
};

struct BiteRuntimeContext {
    epoch_id currentEpoch{0};
    std::shared_ptr<folly::CPUThreadPoolExecutor> threadPoolExecutor{nullptr};
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
        std::vector<transaction_index> failedTransactions;
    };

    /**
     * @brief Stage 1: Parse BITE transactions from the transaction list.
     * Attempts to parse all BITE transactions in the given TransactionList.
     * Caches parsed BiteCiphertext objects in the Transaction objects.
     * Assumes CATs are at the start of the list, followed by regular transactions.
     */
    ParseResult parseAndCacheBITETransactions(
        const TransactionList& txList,
        BiteRuntimeContext& runtimeCtx
    ) const;

    //=================== Stage 2: Merging TE Decryption Shares into AES Keys  =================== //



    //=================== Stage 3: Decrypt transactions from decrypted keys  =================== //

    DecryptedTransactions decryptTransactionsListInParallel(
            TransactionList &_transactionList,
            DecryptedAESKeyList &_aesKeys,
            BiteRuntimeContext& runtimeCtx
    ) const;

    //=================== Stage 4: Ciphertext Validation  =================== //
    struct CiphertextValidationResult {
        std::vector<transaction_index> invalidCiphertextIndices;
        std::vector<std::string> publicDecryptionValues;
        bool allValid() const noexcept {
            return invalidCiphertextIndices.empty();
        }
    };

    CiphertextValidationResult validateCiphertexts(
        const TransactionCiphertextsMap& txsCiphertexts
    ) const;


    ptr<BiteCiphertext> tryGetEncryptedRegularTxFields(
                const ptr<Transaction> &_transaction, epoch_id _currentEpochId) const;

#ifdef BITE2
    ptr<std::vector<ptr<BiteCiphertext>>> tryGetEncryptedCATArgs(
            const ptr<Transaction>& _transaction, epoch_id _currentEpochId ) const;
#endif


    std::vector<uint8_t> BiteEngine::buildRegularTxData(
        const libBLS::TEPublicKey& key,
        const std::vector<uint8_t>& plainData,
        const std::vector<uint8_t>& to
    ) const;

    std::vector<uint8_t> buildCATData(
        const libBLS::TEPublicKey& key,
        size_t numberOfCiphertexts);

};
