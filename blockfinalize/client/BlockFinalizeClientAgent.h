/*
    Copyright (C) 2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file BlockFinalizeClientAgent.h
    @author Stan Kladko
    @date 2019
*/

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

