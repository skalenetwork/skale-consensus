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

class BiteDataField;

class TransactionList;

class EncryptedAESKey;

namespace folly {
class CPUThreadPoolExecutor;
}

class BiteManager {
    Schain &schain;
    bool doRealCrypto = false;
    std::shared_ptr<folly::CPUThreadPoolExecutor> threadPoolExecutor;

    // struct for passing data to the thread pool
    struct AESKeyValidationBatch {

        // vec with pointers to each element in the EncryptedAESKey Map
        std::vector< EncryptedAESKeyList::iterator >* work;
        // start & end indices in the work vec
        size_t startIdx;
        size_t endIdx;

        // output vec with public decryption values. Only contains values for valid transactions, in the order of the input work vec
        ptr<std::vector<ptr<string>>> publicDecryptionValues;    

        // reference to the map of failed transactions in the proposal
        std::map<transaction_index, ConnectionSubStatus>* failedTransactionRef;

        // mutex for thread safety when adding to failedTransactions map
        std::mutex* failedTransactionsMutex;
        bool useThreadSafety;

        explicit AESKeyValidationBatch(
                std::vector< EncryptedAESKeyList::iterator >* _work,
                size_t _startIdx,
                size_t _endIdx,
                ptr<std::vector<ptr<string>>> _publicDecryptionValues,
                std::map<transaction_index, ConnectionSubStatus>* _failedTransactionRef,
                std::mutex* _failedTransactionsMutex,
                bool _useThreadSafety
        ) : work(_work),
            startIdx(_startIdx),
            endIdx(_endIdx),
            publicDecryptionValues(_publicDecryptionValues),
            failedTransactionRef(_failedTransactionRef),
            failedTransactionsMutex(_failedTransactionsMutex),
            useThreadSafety(_useThreadSafety)
        {}
    };

    folly::Unit validateEncryptedAESKeyBatch( AESKeyValidationBatch& batch );

public:
    explicit BiteManager(Schain &_schain);

    static void parseBITETransactions(ptr<BlockProposal> _proposal);

    // this will return a map of failed transactions
    // if none of the transactions fails, the  proposal is set with decryption shares


    [[nodiscard]][[nodiscard]] ptr<AESKeyDecryptionShareList> getDecryptionSharesForProposal(
            ptr<BlockProposal> _proposal);

    [[nodiscard]] Schain *getSchain() const {
        return &schain;
    }

    [[nodiscard]] std::shared_ptr<folly::CPUThreadPoolExecutor> getExecutor() {
        return threadPoolExecutor;
    }

    [[nodiscard]] ptr<vector<ptr<AESKeyDecryptionShare> > > getDecryptionSharesFromAESKeys(
            ptr<BlockProposal> _proposal,
            schain_index _decryptorIndex);

    [[nodiscard]] ptr<DecryptedTransactionFieldsMap> verifyAndDecryptTransactionList(TransactionList &_transactionList,
                                                                                     DecryptedAESKeyList &_aesKeys);

    [[nodiscard]] ptr<AESKeyDecryptionShare> createAESDecryptionShare(const string& _aesKeyDecryptionShare,
                                                                      schain_index _decryptorIndex,
                                                                      bool _decryptionFailed);

    [[nodiscard]] ptr<AESKeyDecryptionShareSet> createAESDecryptionShareSet(
            block_id _blockId, transaction_index _transactionIndex);

    // TODO - change the name of this method
    [[nodiscard]] DecryptedTransactionFields decryptFields(const ptr<BiteDataField> &bite, DecryptedAESKey &_key) const;

    void corruptFromTimeToTime(shared_ptr<vector<unsigned char> > result);

    [[nodiscard]] ptr<vector<uint8_t> > teEncryptDataAndToAddress(const vector<uint8_t> &_data,
                                                                  const vector<uint8_t> &_to);


    void callSGXToCreateMyDecryptionSharesForProposalTransactions(
            ptr<BlockProposal> _proposal);


    [[nodiscard]] bool isRealCryptoEnabled() const;

    void computeAndValidateSGXAESKeyBatch(ptr<BlockProposal> _proposal);
};
