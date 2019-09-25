/*
    Copyright (C) 2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file BlockProposalPusherAgent.h
    @author Stan Kladko
    @date 2019
*/

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


    explicit BlockProposalClientAgent(Schain& _sChain);


    void sendItemImpl(ptr<BlockProposal> &_proposal, shared_ptr<ClientSocket> &socket, schain_index _destIndex,
                      node_id _dstNodeId);

};

