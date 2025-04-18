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

    ConnectionSubStatus verifyAndCreateDecryptionSharesForProposalTransactions(
        const ptr<BlockProposal> &_proposal);

    std::pair<ptr<AESKeyDecryptionShareList>, ConnectionSubStatus> decryptBiteDataFields(
        block_id _blockId, schain_index _proposerIndex,
        const std::map<transaction_index, ptr<BiteDataField> > &_biteDataFields);

    ptr<vector<ptr<AESKeyDecryptionShare> > > decryptAESKeys(vector<ptr<BiteDataField> > &_dataFields);


    ptr<vector<ptr<AESKeyDecryptionShare>>> decryptAESKeyShareBatch(vector<ptr<EncryptedAESKey> >& _encryptedAESKeys,
                                                  schain_index _decryptorIndex);

    ptr<DecryptedTransactionDataFields> verifyAndDecryptTransactionList(TransactionList &_transactionList,
                                                                        DecryptedAESKeyList &_aesKeys);

    ptr<AESKeyDecryptionShare> createAESDecryptionShare(string _aesKeyDecryptionShare,
                                                               schain_index _decryptorIndex,
                                                               bool _decryptionFailed);

    ptr<AESKeyDecryptionShareSet> createAESDecryptionShareSet(block_id _blockId, transaction_index _transactionIndex);

    ptr<vector<uint8_t>> decryptDataField(const ptr<BiteDataField> &bite,  DecryptedAESKey& _key) const;

    void corruptFromTimeToTime(shared_ptr<vector<unsigned char>> result);

    ptr<vector<uint8_t> > teEncryptData(const vector<uint8_t> &_data);
};
