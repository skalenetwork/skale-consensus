#pragma  once

#include "../SkaleConfig.h"

#include "DataStructure.h"

class Schain;
class Transaction;
class PartialHashesList;
class TransactionList;


class BlockProposal : public DataStructure {



protected:

    schain_id schainID;
    block_id blockID;
    schain_index proposerIndex;
    node_id proposerNodeID;


    transaction_count transactionCount;
    uint64_t  timeStamp = 0;

protected:
    ptr<TransactionList> transactionList;
    ptr< SHAHash > hash = nullptr;


    void calculateHash();



    BlockProposal(uint64_t _timeStamp);

    BlockProposal(Schain &_sChain, block_id _blockID, schain_index _proposerIndex,
                  ptr<TransactionList> _transactions, uint64_t _timeStamp);


public:

    uint64_t getTimeStamp() const;



    const transaction_count &getTransactionsCount() const;

    const schain_index &getProposerIndex() const;

    const node_id& getProposerNodeID() const;

    ptr<SHAHash> getHash();


    ptr<PartialHashesList> createPartialHashesList();

    ptr<TransactionList> getTransactionList();

    block_id getBlockID() const;

    virtual ~BlockProposal();

    const schain_id &getSchainID() const;

    const transaction_count &getTransactionCount() const;



};

