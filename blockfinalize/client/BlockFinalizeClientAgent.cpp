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

    @file BlockFinalizeClientAgent.cpp
    @author Stan Kladko
    @date 2019
*/

#include "../../SkaleConfig.h"
#include "../../Log.h"
#include "../../Agent.h"
#include "../../exceptions/FatalError.h"
#include "../../thirdparty/json.hpp"

#include "../../crypto/SHAHash.h"
#include "../../crypto/BLSSigShare.h"
#include "../../abstracttcpserver/ConnectionStatus.h"
#include "../../datastructures/PartialHashesList.h"
#include "../../datastructures/Transaction.h"
#include "../../datastructures/TransactionList.h"


#include "../../node/Node.h"
#include "../../chains/Schain.h"
#include "../../network/IO.h"
#include "../../network/TransportNetwork.h"
#include "../../pendingqueue/PendingTransactionsAgent.h"
#include "../../headers/BlockProposalHeader.h"
#include "../../network/ClientSocket.h"
#include "../../node/Node.h"
#include "../../exceptions/NetworkProtocolException.h"
#include "../../network/Connection.h"
#include "../../headers/MissingTransactionsRequestHeader.h"
#include "../../headers/MissingTransactionsResponseHeader.h"
#include "../../headers/BlockFinalizeRequestHeader.h"
#include "../../datastructures/BlockProposal.h"
#include "../../datastructures/CommittedBlock.h"

#include "../../exceptions/ExitRequestedException.h"
#include "../../exceptions/PingException.h"
#include "../../abstracttcpclient/AbstractClientAgent.h"
#include "BlockFinalizeClientThreadPool.h"
#include "BlockFinalizeClientAgent.h"


BlockFinalizeClientAgent::BlockFinalizeClientAgent(Schain &_sChain) : AbstractClientAgent(_sChain, PROPOSAL) {
    try {
        LOG(debug, "Constructing blockFinalizeClientAgent");

        this->blockFinalizeThreadPool = make_shared<BlockFinalizeClientThreadPool>(
                num_threads((uint64_t) _sChain.getNodeCount()), this);
        blockFinalizeThreadPool->startService();
    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }
}


nlohmann::json BlockFinalizeClientAgent::readProposalResponseHeader(ptr<ClientSocket> _socket) {
    return sChain->getIo()->readJsonHeader(_socket->getDescriptor(), "Read proposal resp");
}


void BlockFinalizeClientAgent::sendItemImpl(ptr<BlockProposal> &_proposal, shared_ptr<ClientSocket> &socket,
                                            schain_index _destIndex,
                                            node_id _dstNodeId) {

    LOG(trace, "Proposal step 0: Starting block proposal");


    auto committedBlock = dynamic_pointer_cast<CommittedBlock>(_proposal);

    ASSERT(committedBlock);

    ptr<Header> header = make_shared<BlockFinalizeRequestHeader>(*sChain, committedBlock,
                                                                 _proposal->getProposerIndex());


    try {
        getSchain()->getIo()->writeHeader(socket, header);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(NetworkProtocolException("Could not write header", __CLASS_NAME__));
    }


    LOG(trace, "Proposal step 1: wrote proposal header");

    auto response = sChain->getIo()->readJsonHeader(socket->getDescriptor(), "Read proposal resp");


    LOG(trace, "Proposal step 2: read proposal response");


    auto status = (ConnectionStatus) Header::getUint64(response, "status");
    // auto substatus = (ConnectionSubStatus) Header::getUint64(response, "substatus");


    if (status != CONNECTION_SUCCESS) {
        LOG(trace, "Proposal Server terminated proposal push");
        return;
    }

    ptr<BLSSigShare> signatureShare;

    try {

        signatureShare = getBLSSignatureShare(response, committedBlock->getBlockID(), _destIndex,
                                              _dstNodeId);
    } catch (...) {
        throw_with_nested(NetworkProtocolException("Could not read signature share from response", __CLASS_NAME__));
    }

    sChain->sigShareArrived(signatureShare);

    LOG(trace, "Proposal step 6: sent missing transactions");
}


ptr<BLSSigShare> BlockFinalizeClientAgent::getBLSSignatureShare(nlohmann::json _json,
                                                                block_id _blockID, schain_index _signerIndex,
                                                                node_id _signerNodeId) {
    auto s = Header::getString(_json, "sigShare");
    return make_shared<BLSSigShare>(s, getSchain()->getSchainID(), _blockID, _signerIndex, _signerNodeId);
}
