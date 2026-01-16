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
    bool doRealCrypto = false;
    std::shared_ptr<folly::CPUThreadPoolExecutor> threadPoolExecutor;

public:
    explicit BiteManager(Schain &_schain);
    ~BiteManager();

    // =============== Transaction Parsing Calls =============== //

    // Runs through all transactions in the proposal and tries to parse all BITE transactions.
    // This includes both:
    // 1) BITE1 transactions - with both 'to' and 'data' fields encrypted
    // 2) BITE2 transactions - with only function arguments encrypted, placed in the data field.
    //
    // Unparsable transactions will be added to failedTransactions.
    // Transactions starting from the magic number but with incorrect format will be added
    // to failedTransactions.
    static void parseBITETransactions(ptr<BlockProposal> _proposal);

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

    // this will return a map of failed transactions
    // if none of the transactions fails, the  proposal is set with decryption shares
    [[nodiscard]] ptr<AESKeyDecryptionShareList> getDecryptionSharesForProposal(
            ptr<BlockProposal> _proposal);

    [[nodiscard]] Schain *getSchain() const {
        return &schain;
    }

    [[nodiscard]] std::shared_ptr<folly::CPUThreadPoolExecutor> getExecutor() {
        return threadPoolExecutor;
    }

    [[nodiscard]] ptr<vector<ptr<AESKeyDecryptionShares> > > getDecryptionSharesFromAESKeys(
            ptr<BlockProposal> _proposal,
            schain_index _decryptorIndex);

    [[nodiscard]] DecryptedTransactions verifyAndDecryptTransactionList(const TransactionList &_transactionList,
                                                                                     const DecryptedAESKeyList &_aesKeys);


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

    void corruptFromTimeToTime(shared_ptr<vector<unsigned char> > result);


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
    [[nodiscard]] ptr<vector<uint8_t> > generateEncryptedCATData();
    
    /**
     * @brief Generates a CAT transaction with no encrypted arguments (empty ciphertexts).
     * Only plain arguments are included.
     */
    [[nodiscard]] ptr<vector<uint8_t> > generateEmptyCATData();
#endif

private:
    // Decrypts a single ciphertext using the provided AES key 
    vector<uint8_t> decryptCiphertext(const ptr<BiteCiphertext> &_bite, const DecryptedAESKey &_decryptedAESKey) const;

    // Parses decrypted data as a regular transaction, extracting 'data' and 'to' fields
    DecryptedRegularTxFields parseDecryptedDataAsRegularTx(const vector<uint8_t> &_data) const;

    // Parses decrypted data as a set of CAT function arguments
    DecryptedCATArgs parseDecryptedDataAsCATArgs(const vector<uint8_t> &_data) const;

    ptr<vector<uint8_t>> encryptData(const vector<uint8_t>& data);

    void stopAndDestroyThreadPoolExecutor();

};
