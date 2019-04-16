#pragma once


#include "BlockProposal.h"



class ReceivedBlockProposal : public BlockProposal{
public:
    ReceivedBlockProposal(Schain &_sChain, const block_id &_blockID, const schain_index &_proposerIndex,
                          const ptr<TransactionList> & _transactions, const uint64_t &_timeStamp);


    static uint64_t getTotalObjects() {
        return totalObjects;
    }

    virtual ~ReceivedBlockProposal();

private:

    static atomic<uint64_t>  totalObjects;

};
