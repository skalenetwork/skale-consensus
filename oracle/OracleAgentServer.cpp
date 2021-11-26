/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file OracleAgentServer.cpp
    @author Stan Kladko
    @date 2018-
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"

#include "blockfinalize/client/BlockFinalizeDownloader.h"
#include "blockfinalize/client/BlockFinalizeDownloaderThreadPool.h"
#include "blockproposal/pusher/BlockProposalClientAgent.h"
#include "chains/Schain.h"
#include "crypto/BLAKE3Hash.h"
#include "crypto/ThresholdSigShare.h"
#include "crypto/CryptoManager.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/BooleanProposalVector.h"
#include "datastructures/TransactionList.h"
#include "db/BlockDB.h"
#include "db/BlockProposalDB.h"
#include "db/BlockSigShareDB.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/InvalidStateException.h"
#include "messages/ConsensusProposalMessage.h"
#include "messages/InternalMessageEnvelope.h"
#include "messages/NetworkMessage.h"
#include "messages/NetworkMessageEnvelope.h"
#include "messages/ParentMessage.h"
#include "network/Network.h"
#include "node/Node.h"
#include "node/NodeInfo.h"
#include "pendingqueue/PendingTransactionsAgent.h"

#include "thirdparty/lrucache.hpp"

#include "utils/Time.h"
#include "protocols/ProtocolInstance.h"
#include "OracleAgentServer.h"
#include "OracleRequestBroadcastMessage.h"


OracleAgentServer::OracleAgentServer(Schain &_schain) : ProtocolInstance(
        ORACLE, _schain), Agent(_schain, true), requestCounter(0) {
    for (int i = 0; i < NUM_ORACLE_THREADS; i++) {
        incomingQueues.push_back(
                make_shared<BlockingReaderWriterQueue<shared_ptr<MessageEnvelope>>>());
    }
};

void OracleAgentServer::routeAndProcessMessage(const ptr<MessageEnvelope> &_me) {

    CHECK_ARGUMENT(_me);

    CHECK_ARGUMENT(_me->getMessage()->getBlockId() > 0);

    CHECK_STATE(_me->getMessage()->getMessageType() == MSG_ORACLE_REQ_BROADCAST);

    auto value = requestCounter.fetch_add(1);

    this->incomingQueues.at(value %  (uint64_t) NUM_ORACLE_THREADS)->enqueue(_me);

    return;

}

void OracleAgentServer::workerThreadItemSendLoop(OracleAgentServer* _agent) {

    CHECK_STATE(_agent);

    _agent->waitOnGlobalStartBarrier();

}
