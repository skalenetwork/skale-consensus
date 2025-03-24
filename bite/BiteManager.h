#pragma once

#include "abstracttcpserver/ConnectionStatus.h"
class Schain;
class BlockProposal;


class BiteManager {
    Schain& schain;

public:
    explicit BiteManager( Schain& schain );

    ConnectionSubStatus verifyAndDecryptProposalTransactions(
        const ptr< BlockProposal >& _proposal);


};


