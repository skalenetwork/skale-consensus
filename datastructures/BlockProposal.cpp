#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "../crypto/SHAHash.h"
#include "../node/ConsensusEngine.h"
#include "../exceptions/InvalidArgumentException.h"
#include "../exceptions/OldBlockIDException.h"
#include "Transaction.h"
#include "TransactionList.h"
#include "PartialHashesList.h"
#include "../chains/Schain.h"
#include "../pendingqueue/PendingTransactionsAgent.h"

#include "BlockProposal.h"


using namespace std;

ptr<SHAHash> BlockProposal::getHash() {
    ASSERT(hash);
    return hash;

}


void BlockProposal::calculateHash() {


    CryptoPP::SHA256 sha3;

    sha3.Update(reinterpret_cast < uint8_t * > ( &proposerIndex), sizeof(proposerIndex));
    sha3.Update(reinterpret_cast < uint8_t * > ( &schainID      ), sizeof(schainID));
    sha3.Update(reinterpret_cast < uint8_t * > ( &blockID       ), sizeof(blockID));
    sha3.Update(reinterpret_cast < uint8_t * > ( &transactionCount ), sizeof(transactionCount));
    sha3.Update(reinterpret_cast < uint8_t * > ( &timeStamp ), sizeof(timeStamp));


    for (uint64_t i = 0; i < transactionCount; i++) {
        auto t = transactionList->getItems();
        ASSERT((*t)[i]);
        sha3.Update((*t)[i]->getHash()->data(), SHA3_HASH_LEN);
    }

    auto buf = make_shared<array<uint8_t, SHA3_HASH_LEN>>();
    sha3.Final(buf->data());
    hash = make_shared<SHAHash>(buf);
};


BlockProposal::BlockProposal(uint64_t _timeStamp) : timeStamp(_timeStamp) {
    proposerNodeID = 0;
};

BlockProposal::BlockProposal(Schain &_sChain, block_id _blockID, schain_index _proposerIndex,
                             ptr<TransactionList> _transactions, uint64_t _timeStamp) : schainID(_sChain.getSchainID()),
                                                                                        blockID(_blockID),
                                                                                        proposerIndex(_proposerIndex),
                                                                                        timeStamp(_timeStamp),
                                                                                        transactionList(_transactions) {
    proposerNodeID = _sChain.getNodeID(_proposerIndex);

    ASSERT(timeStamp > MODERN_TIME);
    transactionCount = transactionList->getItems()->size();
    calculateHash();

}


ptr<PartialHashesList> BlockProposal::createPartialHashesList() {


    auto s = (uint64_t) this->transactionCount * PARTIAL_SHA_HASH_LEN;
    auto t = transactionList->getItems();


    if (s > MAX_BUFFER_SIZE) {
        throw InvalidArgumentException("Buffer size too large", __CLASS_NAME__);
    }

    auto partialHashes = make_shared<vector<uint8_t>>(s);

    for (uint64_t i = 0; i < transactionCount; i++) {

        for (size_t j = 0; j < PARTIAL_SHA_HASH_LEN; j++) {
            (*partialHashes)[i * PARTIAL_SHA_HASH_LEN + j] = (*(*t)[i]->getHash()).at(j);
        }
    }

    return make_shared<PartialHashesList>((transaction_count) transactionCount, partialHashes);

}

BlockProposal::~BlockProposal() {

}

block_id BlockProposal::getBlockID() const {
    return blockID;
}

const transaction_count &BlockProposal::getTransactionsCount() const {
    return transactionCount;
}

const schain_index &BlockProposal::getProposerIndex() const {
    return proposerIndex;
}


const node_id& BlockProposal::getProposerNodeID() const {
    return proposerNodeID;
}


ptr<TransactionList> BlockProposal::getTransactionList() {
    return transactionList;
}

const schain_id &BlockProposal::getSchainID() const {
    return schainID;
}

const transaction_count &BlockProposal::getTransactionCount() const {
    return transactionCount;
}

uint64_t BlockProposal::getTimeStamp() const {
    return timeStamp;
}


