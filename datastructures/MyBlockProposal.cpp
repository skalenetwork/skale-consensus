#include "../SkaleConfig.h"
#include "Transaction.h"
#include "MyBlockProposal.h"

MyBlockProposal::MyBlockProposal(Schain &_sChain, const block_id &_blockID, const schain_index &_proposerIndex,
                                 const ptr<TransactionList>_transactions, uint64_t _timeStamp)
        : BlockProposal(_sChain, _blockID, _proposerIndex, _transactions, _timeStamp) {
    totalObjects++;
};




atomic<uint64_t>  MyBlockProposal::totalObjects(0);

MyBlockProposal::~MyBlockProposal() {
    totalObjects--;

}

