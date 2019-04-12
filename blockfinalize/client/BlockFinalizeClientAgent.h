#pragma once

#include "../../abstracttcpclient/AbstractClientAgent.h"
#include "../../pendingqueue/PendingTransactionsAgent.h"


class ClientSocket;
class Schain;
class BlockFinalizeClientThreadPool;
class BlockProposal;
class MissingTransactionsRequestHeader;


class BlockFinalizeClientAgent : public AbstractClientAgent {


    friend class BlockFinalizeClientThreadPool;

    nlohmann::json readProposalResponseHeader(ptr<ClientSocket> _socket);


    void sendItem(ptr<BlockProposal> _proposal, schain_index _dstIndex, node_id _dstNodeId);


    static void workerThreadItemSendLoop(AbstractClientAgent *agent);


    ptr<BLSSigShare> getBLSSignatureShare(nlohmann::json _json,
       block_id _blockID, schain_index _signerIndex, node_id _signerNodeId);





public:

    ptr<BlockFinalizeClientThreadPool> blockFinalizeThreadPool = nullptr;


    explicit BlockFinalizeClientAgent(Schain& subChain_);


    void sendItemImpl(ptr<BlockProposal> &_proposal, shared_ptr<ClientSocket> &socket, schain_index _destIndex,
                      node_id _dstNodeId);

};

