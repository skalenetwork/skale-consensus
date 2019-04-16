#pragma once

#include "../../abstracttcpclient/AbstractClientAgent.h"
#include "../../pendingqueue/PendingTransactionsAgent.h"


class ClientSocket;
class Schain;
class BlockProposalPusherThreadPool;
class BlockProposal;
class MissingTransactionsRequestHeader;


class BlockProposalClientAgent : public AbstractClientAgent {

    friend class BlockProposalPusherThreadPool;

    nlohmann::json readProposalResponseHeader(ptr<ClientSocket> _socket);


    void sendItem(ptr<BlockProposal> _proposal, schain_index _dstIndex, node_id _dstNodeId);


    static void workerThreadItemSendLoop(AbstractClientAgent *agent);


    ptr<MissingTransactionsRequestHeader> readAndProcessMissingTransactionsRequestHeader(ptr<ClientSocket> _socket);




    ptr<unordered_set<ptr<partial_sha_hash>, PendingTransactionsAgent::Hasher, PendingTransactionsAgent::Equal>>
    readMissingHashes(ptr<ClientSocket> _socket, uint64_t _count);


public:

    ptr<BlockProposalPusherThreadPool> blockProposalThreadPool = nullptr;


    explicit BlockProposalClientAgent(Schain& subChain_);


    void sendItemImpl(ptr<BlockProposal> &_proposal, shared_ptr<ClientSocket> &socket, schain_index _destIndex,
                      node_id _dstNodeId);

};

