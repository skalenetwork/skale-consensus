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
class BITEDataField;

class BiteManager {
    Schain &schain;
    bool doRealCrypto = false;

public:
    explicit BiteManager(Schain &schain);

    ConnectionSubStatus verifyAndDecryptProposalTransactions(
        const ptr<BlockProposal> &_proposal);

    std::pair<ptr<AESKeyDecryptionShareList>, ConnectionSubStatus> decryptBiteDataFields(
        block_id _blockId, schain_index _proposerIndex,  const std::map<transaction_index, ptr<BITEDataField> > &_biteDataFields);

    ptr<vector<ptr<AESKeyDecryptionShare>>> decryptAESKeys(vector<ptr<BITEDataField>> &_dataFiuelds);

    void verifyAndDecryptBlockTransactions(const ptr<CommittedBlock> &_block);

    static ptr<AESKeyDecryptionShare> createAESDecryptionShare(string _aesKeyDecryptionShare, schain_index _decryptorIndex,
                                                        bool _decryptionFailed);

     ptr<AESKeyDecryptionShareSet> createAESDecryptionShareSet(block_id _blockId, transaction_index _transactionIndex);
};
