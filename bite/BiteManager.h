#pragma once

#include <memory>
#include <memory>
#include <vector>
#include <crypto/AESKeyDecryptionShare.h>
#include <crypto/AESKeyDecryptionShareSet.h>
#include <folly/Unit.h>
#include "bite/BiteEngine.h"

class Schain;

class BlockProposal;
class BiteEngine;

class CommittedBlock;

class DecryptedAESKeyList;

class AESKeyDecryptionShareList;

class BiteCiphertext;

class TransactionList;

class EncryptedAESKey;

class ParsedEthTransaction;

namespace folly {
class CPUThreadPoolExecutor;
}

class BiteManager {
    Schain &schain;
    std::shared_ptr<folly::CPUThreadPoolExecutor> threadPoolExecutor;
    BiteEngine biteEngine;

public:
    explicit BiteManager(Schain &_schain);
    ~BiteManager();

    // =============== Stage 1: Ciphertext Parsing =============== //

    // Runs through all transactions in the proposal and tries to parse all BITE transactions.
    // This includes both:
    // 1) BITE1 transactions - with both 'to' and 'data' fields encrypted
    // 2) BITE2 transactions - with only function arguments encrypted, placed in the data field.
    //
    // Unparsable transactions will be added to failedTransactions.
    // Transactions starting from the magic number but with incorrect format will be added
    // to failedTransactions.
    void parseBITETransactions(ptr<BlockProposal> _proposal);

    // =============== Stage 2: Ciphertext Validation =============== //

    void computeAndValidateSGXAESKeyBatch(ptr<BlockProposal> _proposal);

    // =============== Stage 3: Compute Ciphertext shares =============== //

    /**
     * @brief For a given proposal, computes the decryption shares for all 
     * transactions in SGX synchronously.
     */
    void callSGXToCreateMyDecryptionSharesForProposalTransactions(
            ptr<BlockProposal> _proposal);

    /**
     * @brief Schedules the computation of decryption shares in SGX for the given proposal.
     * If the computation has already started, returns right away.
     */
    void scheduleSGXToCreateMyDecryptionSharesForProposalTransactions(
            ptr<BlockProposal> _proposal);

    // =============== Stage 4: Share merging =============== //

    std::shared_ptr<DecryptedAESKeyList> mergeAESKeys(
        block_id _blockId,
        TransactionCiphertextsMap& _txCiphertexts,
        const std::map<schain_index, std::shared_ptr<AESKeyDecryptionShareList>>& _decryptionShareMap,
        const std::vector<libBLS::TEPublicKeyShare>& _tePublicKeyShares,
        const BiteRuntimeContext& _runtimeCtx
    ) const;

    // =============== Stage 5: Transaction Decryption =============== //

    [[nodiscard]] DecryptedTransactions verifyAndDecryptTransactionList(
        const TransactionList &_transactionList, const DecryptedAESKeyList &_aesKeys,
        epoch_id _epochId, bool _isBite2PatchEnabledForBlock
    );


    // ============== Getters ============== //

    [[nodiscard]] Schain *getSchain() const {
        return &schain;
    }

    [[nodiscard]] std::shared_ptr<folly::CPUThreadPoolExecutor> getExecutor() {
        return threadPoolExecutor;
    }

    // =============== Helpers =============== //

    /**
     * @brief Converts the vector of AESKeys (corresponding to decryption shares) into a mapping of txId -> decryption shares.
     */
    [[nodiscard]] ptr<AESKeyDecryptionShareList> getDecryptionSharesForProposal(
            ptr<BlockProposal> _proposal);


    /**
     * @brief For a given proposal, computes the decrypt shares for all ciphertexts of all transactions.
     */
    [[nodiscard]] ptr<vector<ptr<AESKeyDecryptionShares> > > getDecryptionSharesFromAESKeys(
            ptr<BlockProposal> _proposal,
            schain_index _decryptorIndex);

    /**
     * @brief Builds a vec of all decryptionShares for a given transaction from a serialized string format.
     * @param _aesKeyDecryptionShares - serialized string format of decryption shares. Each share is in string format
     * separated by commas.
     * @param _decryptorIndex - index of the decryptor node that created these shares
     * @param _decryptionFailed - whether decryption failed for this transaction
     * @param _validate - whether to validate the shares during creation (only applicable if using real crypto). 
     * If true and validation fails, an exception is thrown.
     */
    [[nodiscard]] ptr<AESKeyDecryptionShares> createAESDecryptionShares(const string& _aesKeyDecryptionShares,
                                                                      schain_index _decryptorIndex,
                                                                      bool _decryptionFailed,
                                                                      CryptographicValidationMode _validationMode = CryptographicValidationMode::Validate);


    [[nodiscard]] ptr<AESKeyDecryptionShareSet> createAESDecryptionShareSet(
            block_id _blockId, transaction_index _transactionIndex, size_t numberOfCiphertexts);


    // =============== Test Encryption Calls =============== //

    /**
     * @brief Encrypts regular transaction data and to address using BITE1 scheme.
     * @param _data - data field of the transaction
     * @param _to - to address of the transaction
     */
    [[nodiscard]] ptr<vector<uint8_t> > encryptRegularTx(const vector<uint8_t> &_data,
                                                                  const vector<uint8_t> &_to, uint64_t epochId);

    /**
     * @brief Encrypts CTX function arguments using BITE2 scheme.
     * @param _scAddressAadTE - Smart contract address used as AAD for TE validation (real crypto only)
     * @param _data - Returned data follows the format:
     * [
     *      funcSelector,  // 4 bytes
     *      RLP( 
     *          RLP(cipher1, cipher2, ...), 
     *          RLP(plaintext1, plaintext2, ...) 
     *      ),
     * ]
     */
    [[nodiscard]] ptr<vector<uint8_t> > generateEncryptedCTXData(uint64_t epochId, const std::optional<AddressBytes>& scAddressAadTE = std::nullopt);

    /**
     * @brief Generates a CTX transaction with no encrypted arguments (empty ciphertexts).
     * Only plain arguments are included.
     */
    [[nodiscard]] ptr<vector<uint8_t> > generateEmptyCTXData(uint64_t epochId);

private:

    /**
     * @brief For a given proposal, computes the decryption shares for all transactions.
     */
    void computeMyDecryptionSharesForProposalTransactions(ptr<BlockProposal> _proposal);

    void stopAndDestroyThreadPoolExecutor();
};
