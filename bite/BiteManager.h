#pragma once

#include <memory>
#include <memory>
#include <vector>
#include <crypto/AESKeyDecryptionShare.h>
#include <crypto/AESKeyDecryptionShareSet.h>

#include "abstracttcpserver/ConnectionStatus.h"
class Schain;
class BlockProposal;
class CommittedBlock;
class DecryptedAESKeyList;
class AESKeyDecryptionShareList;
class BiteDataField;
class TransactionList;
class EncryptedAESKey;


using DecryptedTransactionDataFields = map<uint64_t, shared_ptr<vector<uint8_t> > >;

class BiteManager {
    Schain &schain;
    bool doRealCrypto = false;

public:
    explicit BiteManager(Schain &_schain);

    // this will return a map of failed transactions
    // if none of the transactions fails, the  proposal is set with decryption shares
    [[nodiscard]] std::map<transaction_index, ConnectionSubStatus>  verifyAndCreateDecryptionSharesForProposalTransactions(
        const ptr<BlockProposal> &_proposal);

    [[nodiscard]] ptr<AESKeyDecryptionShareList> getDecryptionSharesFromDataFieldsMap(
        block_id _blockId, schain_index _proposerIndex,
        const std::map<transaction_index, ptr<BiteDataField> > &_biteDataFields,
        map<transaction_index, ConnectionSubStatus>& _failedTransactions);

    [[nodiscard]] ptr<vector<ptr<AESKeyDecryptionShare> > > getDecryptionSharesFromDataFields(vector<ptr<BiteDataField> > &_dataFields,
        map<transaction_index, ConnectionSubStatus> &_failedTransactions);


    [[nodiscard]]  ptr<vector<ptr<AESKeyDecryptionShare>>> getDecryptionSharesFromAESKeys(vector<ptr<EncryptedAESKey> >& _encryptedAESKeys,
                       schain_index _decryptorIndex, map<transaction_index, ConnectionSubStatus> &_failedTransactions);

    [[nodiscard]]  ptr<DecryptedTransactionDataFields> verifyAndDecryptTransactionList(TransactionList &_transactionList,
                                                                        DecryptedAESKeyList &_aesKeys);

    [[nodiscard]] ptr<AESKeyDecryptionShare> createAESDecryptionShare(string _aesKeyDecryptionShare,
                                                               schain_index _decryptorIndex,
                                                               bool _decryptionFailed);

    [[nodiscard]]  ptr<AESKeyDecryptionShareSet> createAESDecryptionShareSet(block_id _blockId, transaction_index _transactionIndex);

    [[nodiscard]]  ptr<vector<uint8_t>> decryptDataField(const ptr<BiteDataField> &bite,  DecryptedAESKey& _key) const;

    void corruptFromTimeToTime(shared_ptr<vector<unsigned char>> result);

    [[nodiscard]]  ptr<vector<uint8_t> > teEncryptData(const vector<uint8_t> &_data);
};
