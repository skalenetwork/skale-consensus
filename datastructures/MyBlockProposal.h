#pragma once


#include "BlockProposal.h"




class MyBlockProposal : public BlockProposal {
public:
    MyBlockProposal(Schain &_sChain, const block_id &_blockID, const schain_index &_proposerIndex,
                    const ptr<TransactionList> _transactions, uint64_t _timeStamp);


    static uint64_t getTotalObjects() {
        return totalObjects;
    }

private:

    static atomic<uint64_t>  totalObjects;
public:
    virtual ~MyBlockProposal();

};

