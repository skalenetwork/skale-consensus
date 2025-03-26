#pragma once

#include "abstracttcpserver/ConnectionStatus.h"
class Schain;
class BlockProposal;
class CommittedBlock;


class BiteManager {
    Schain& schain;
    bool doRealCrypto = false;

public:
    explicit BiteManager( Schain& schain );

    static ConnectionSubStatus verifyAndDecryptProposalTransactions(const ptr< BlockProposal >& _proposal);

    void verifyAndDecryptBlockTransactions(const ptr<CommittedBlock> &_block);

};


