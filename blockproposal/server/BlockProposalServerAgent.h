#pragma once

#include "../../abstracttcpserver/AbstractServerAgent.h"
#include "../../pendingqueue/PendingTransactionsAgent.h"

class BlockProposalWorkerThreadPool;

class Transaction;

class TransactionList;


class Comparator {
public:
    bool operator()(const ptr<partial_sha_hash> &a,
                    const ptr<partial_sha_hash> &b) const {
        for (size_t i = 0; i < PARTIAL_SHA_HASH_LEN; i++) {
            if ((*a)[i] < (*b)[i])
                return false;
            if ((*b)[i] < (*a)[i])
                return true;
        }
        return false;
    }
};


class BlockProposalServerAgent : public AbstractServerAgent {

    ptr<BlockProposalWorkerThreadPool> blockProposalWorkerThreadPool;


    void processProposalRequest(ptr<Connection> _connection, nlohmann::json _proposalRequest);


    void processFinalizeRequest(ptr<Connection> _connection, nlohmann::json _finalizeRequest);


public:
    BlockProposalServerAgent(Schain &_schain, ptr<TCPServerSocket> _s);

    ~BlockProposalServerAgent() override;

    ptr<unordered_map<ptr<partial_sha_hash>, ptr<Transaction>, PendingTransactionsAgent::Hasher, PendingTransactionsAgent::Equal>>
    readMissingTransactions(ptr<Connection> connectionEnvelope_, nlohmann::json missingTransactionsResponseHeader);


    pair<ptr<map<uint64_t, ptr<Transaction>>>,
            ptr<map<uint64_t, ptr<partial_sha_hash>>>> getPresentAndMissingTransactions(Schain &subChain_,
                                                                                         ptr<Header>,
                                                                                         ptr<PartialHashesList> _phm);


    BlockProposalWorkerThreadPool *getBlockProposalWorkerThreadPool() const;

    void checkForOldBlock(const block_id &_blockID);

    ptr<Header>
    createProposalResponseHeader(ptr<Connection> _connectionEnvelope,
                                 nlohmann::json _jsonRequest);

    ptr<Header>
    createFinalizeResponseHeader(ptr<Connection> _connectionEnvelope,
                                 nlohmann::json _jsonRequest);


    nlohmann::json
    readMissingTransactionsResponseHeader(ptr<Connection> _connectionEnvelope);


    void processNextAvailableConnection(ptr<Connection> _connection) override;

};
