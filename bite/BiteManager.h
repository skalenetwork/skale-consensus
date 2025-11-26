#pragma once

#include <memory>
#include <memory>
#include <vector>
#include <crypto/AESKeyDecryptionShare.h>
#include <crypto/AESKeyDecryptionShareSet.h>
#include "node/ConsensusInterface.h"
#include "abstracttcpserver/ConnectionStatus.h"
#include <folly/Unit.h>

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

    // =============== Transaction Parsing Calls =============== //

    // Runs through all transactions in the proposal and tries to parse all BITE transactions.
    // This includes both:
    // 1) BITE1 transactions - with both 'to' and 'data' fields encrypted
    // 2) BITE2 transactions - with only function arguments encrypted, placed in the data field.
    //
    // Unparsable transactions will be added to failedTransactions.
    // Transactions starting from the magic number but with incorrect format will be added
    // to failedTransactions.
    void parseBITETransactions(ptr<BlockProposal> _proposal);

    // Tries to match the TO field to BITE1 magic number. If it matches - tries to parse the BITE1 data field.
    // If parsing is successful - returns BiteCiphertext object, else returns 'nullopt'
    static ptr<BiteCiphertext> tryGetEncryptedRegularTxFields(
            const ptr<Transaction> &_transaction, epoch_id _currentEpochId);

#ifdef BITE2
    /**
     * @brief Tries to match the beginning of DATA field to BITE2 function selector.
     * If it matches - tries to parse the BITE2 data field(s) for encrypted call arguments.
     * If parsing is successful - returns vector of BiteCiphertext objects, else returns 'nullopt'
     * @throws if it matches the function selector but fails to parse the rest of the data
     */
    static ptr<std::vector<ptr<BiteCiphertext>>> tryGetEncryptedCATArgs(
            const ptr<Transaction> &_transaction, epoch_id _currentEpochId);
#endif

    // =============== Ciphertext Parsing =============== //

    /**
     * @brief Converts the vector of AESKeys (corresponding to decryption shares) into a mapping of txId -> decryption shares.
     */
    [[nodiscard]] ptr<AESKeyDecryptionShareList> getDecryptionSharesForProposal(
            ptr<BlockProposal> _proposal);

    [[nodiscard]] Schain *getSchain() const {
        return &schain;
    }

    [[nodiscard]] std::shared_ptr<folly::CPUThreadPoolExecutor> getExecutor() {
        return threadPoolExecutor;
    }

    /**
     * @brief For a given proposal, computes the decrypt shares for all ciphertexts of all transactions.
     */
    [[nodiscard]] ptr<vector<ptr<AESKeyDecryptionShares> > > getDecryptionSharesFromAESKeys(
            ptr<BlockProposal> _proposal,
            schain_index _decryptorIndex);

    [[nodiscard]] DecryptedTransactions verifyAndDecryptTransactionList(TransactionList &_transactionList,
                                                                                     DecryptedAESKeyList &_aesKeys);

    static DecryptedTransactions decryptTransactionsListInParallel(
            TransactionList &_transactionList,
            DecryptedAESKeyList &_aesKeys,
            epoch_id currentEpochId,
            std::shared_ptr<folly::CPUThreadPoolExecutor> threadPoolExecutor,
            bool doRealCrypto
    );


    /**
     * @brief Builds a vec of all decryptionShares for a given transaction from a serialized string format.
     * @param _aesKeyDecryptionShares - serialized string format of decryption shares. Each share is in string format
     * separated by commas.
     * @param _decryptorIndex - index of the decryptor node that created these shares
     * @param _decryptionFailed - whether decryption failed for this transaction
     */
    [[nodiscard]] ptr<AESKeyDecryptionShares> createAESDecryptionShares(const string& _aesKeyDecryptionShares,
                                                                      schain_index _decryptorIndex,
                                                                      bool _decryptionFailed);



    [[nodiscard]] ptr<AESKeyDecryptionShareSet> createAESDecryptionShareSet(
            block_id _blockId, transaction_index _transactionIndex, size_t numberOfCiphertexts);


    void callSGXToCreateMyDecryptionSharesForProposalTransactions(
            ptr<BlockProposal> _proposal);


    [[nodiscard]] bool isRealCryptoEnabled() const;

    void computeAndValidateSGXAESKeyBatch(ptr<BlockProposal> _proposal);


    // =============== Test Encryption Calls =============== //

    /**
     * @brief Encrypts regular transaction data and to address using BITE1 scheme.
     * @param _data - data field of the transaction
     * @param _to - to address of the transaction
     */
    [[nodiscard]] ptr<vector<uint8_t> > encryptRegularTx(const vector<uint8_t> &_data,
                                                                  const vector<uint8_t> &_to);


#ifdef BITE2
    /**
     * @brief Encrypts CAT function arguments using BITE2 scheme.
     * @param _data - Returned data follows the format:
     * [
     *      funcSelector,  // 4 bytes
     *      RLP( 
     *          RLP(cipher1, cipher2, ...), 
     *          RLP(plaintext1, plaintext2, ...) 
     *      ),
     * ]
     */
    ptr<vector<uint8_t> > generateEncryptedCATData();

    ptr<vector<uint8_t> > generateEncryptedCATDataWithCount(size_t numberOfCiphertexts, bool useRealCrypto);
#endif

};
