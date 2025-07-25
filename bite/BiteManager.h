#pragma once

#include <memory>
#include <memory>
#include <vector>
#include <crypto/AESKeyDecryptionShare.h>
#include <crypto/AESKeyDecryptionShareSet.h>
#include "node/ConsensusInterface.h"
#include "abstracttcpserver/ConnectionStatus.h"

class Schain;

class BlockProposal;

class CommittedBlock;

class DecryptedAESKeyList;

class AESKeyDecryptionShareList;

class BiteDataField;

class TransactionList;

class EncryptedAESKey;

class BiteManager {
    Schain &schain;
    bool doRealCrypto = false;

public:
    explicit BiteManager(Schain &_schain);

    static void parseBITETransactions(ptr<BlockProposal> _proposal);

    // this will return a map of failed transactions
    // if none of the transactions fails, the  proposal is set with decryption shares

    [[nodiscard]] ptr<vector<ptr<AESKeyDecryptionShare> > > getDecryptionSharesFromDataFields(
            vector<ptr<BiteDataField> > &_dataFields, map<transaction_index, ConnectionSubStatus> &_failedTransactions);


    [[nodiscard]][[nodiscard]] ptr<AESKeyDecryptionShareList> getDecryptionSharesFromDataFieldsMap(
            ptr<BlockProposal> _proposal);

    [[nodiscard]] Schain *getSchain() const {
        return &schain;
    }


    [[nodiscard]] ptr<vector<ptr<AESKeyDecryptionShare> > > getDecryptionSharesFromAESKeys(
            vector<ptr<EncryptedAESKey> > &_encryptedAESKeys,
            schain_index _decryptorIndex, map<transaction_index, ConnectionSubStatus> &_failedTransactions);

    [[nodiscard]] ptr<DecryptedTransactionFieldsMap> verifyAndDecryptTransactionList(TransactionList &_transactionList,
                                                                                     DecryptedAESKeyList &_aesKeys);

    [[nodiscard]] ptr<AESKeyDecryptionShare> createAESDecryptionShare(string _aesKeyDecryptionShare,
                                                                      schain_index _decryptorIndex,
                                                                      bool _decryptionFailed);

    [[nodiscard]] ptr<AESKeyDecryptionShareSet> createAESDecryptionShareSet(
            block_id _blockId, transaction_index _transactionIndex);

    // TODO - change the name of this method
    [[nodiscard]] DecryptedTransactionFields decryptFields(const ptr<BiteDataField> &bite, DecryptedAESKey &_key) const;

    void corruptFromTimeToTime(shared_ptr<vector<unsigned char> > result);

    [[nodiscard]] ptr<vector<uint8_t> > teEncryptDataAndToAddress(const vector<uint8_t> &_data,
                                                                  const vector<uint8_t> &_to);


    [[nodiscard]] map<transaction_index, ConnectionSubStatus> verifyAndCreateMyDecryptionSharesForProposalTransactions(
            ptr<BlockProposal> _proposal);


    [[nodiscard]] bool isRealCryptoEnabled() const;

    [[nodiscard]] static ptr<vector<ptr<string>>>
    computeAndValidateSGXAESKeyBatch(vector<ptr<EncryptedAESKey>> &_encryptedAESKeys,
                                     map<transaction_index, ConnectionSubStatus> &_failedTransactions);
};
