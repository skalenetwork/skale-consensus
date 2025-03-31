#pragma once

#include <memory>
#include <vector>
#include <crypto/AESKeyDecryptionShare.h>

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
        block_id _blockId, const std::map<transaction_index, ptr<BITEDataField> > &_biteDataFields);

    ptr<vector<ptr<AESKeyDecryptionShare>>> decryptAESKeys(vector<ptr<BITEDataField>> &_dataFiuelds);

    void verifyAndDecryptBlockTransactions(const ptr<CommittedBlock> &_block);

    ptr<DecryptedAESKeyList> mergeDecryptionSharesSetFromDB(ptr<map<schain_index, string> > &_decryptionShareList);
};
