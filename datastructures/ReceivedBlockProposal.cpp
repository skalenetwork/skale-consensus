#include "../SkaleConfig.h"
#include "../crypto/SHAHash.h"
#include "ReceivedBlockProposal.h"

ReceivedBlockProposal::ReceivedBlockProposal(Schain &_sChain, const block_id &_blockID,
                                             const schain_index &_proposerIndex,
                                             const ptr<TransactionList> &_transactions,
                                             const uint64_t &_timeStamp) : BlockProposal(_sChain, _blockID,
                                                                                         _proposerIndex, _transactions,
                                                                                         _timeStamp) {
    totalObjects++;
}




atomic<uint64_t>  ReceivedBlockProposal::totalObjects(0);

ReceivedBlockProposal::~ReceivedBlockProposal() {
    totalObjects--;
}


