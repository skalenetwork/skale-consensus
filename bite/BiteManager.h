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
    std::unique_ptr<folly::CPUThreadPoolExecutor> threadPoolExecutor;

public:
    explicit BiteManager(Schain &_schain);

    ~BiteManager();

    static void parseBITETransactions(ptr<BlockProposal> _proposal);

    // this will return a map of failed transactions
    // if none of the transactions fails, the  proposal is set with decryption shares


    [[nodiscard]][[nodiscard]] ptr<AESKeyDecryptionShareList> getDecryptionSharesForProposal(
            ptr<BlockProposal> _proposal);

    [[nodiscard]] Schain *getSchain() const {
        return &schain;
    }

    [[nodiscard]] const std::unique_ptr<folly::CPUThreadPoolExecutor>& getExecutor() const {
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
private:
    void stopAndDestroyThreadPoolExecutor();
};
