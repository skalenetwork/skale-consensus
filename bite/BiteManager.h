#pragma once

#include "abstracttcpserver/ConnectionStatus.h"
class Schain;
class BlockProposal;
class CommittedBlock;
class DecryptedAESKeyList;


class BiteManager {
    Schain& schain;
    bool doRealCrypto = false;

public:
    explicit BiteManager( Schain& schain );

    static ConnectionSubStatus verifyAndDecryptProposalTransactions(const ptr< BlockProposal >& _proposal);

    void verifyAndDecryptBlockTransactions(const ptr<CommittedBlock> &_block);


    ptr<DecryptedAESKeyList> mergeDecryptionSharesSetFromDB(ptr<map<schain_index, string>>& _decryptionShareList);


};


