/*
    Copyright (C) 2021- SKALE Labs

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

    @file OracleServerAgent.cpp
    @author Stan Kladko
    @date 2021-
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"

#include "blockfinalize/client/BlockFinalizeDownloader.h"
#include "blockfinalize/client/BlockFinalizeDownloaderThreadPool.h"
#include "blockproposal/pusher/BlockProposalClientAgent.h"
#include "Agent.h"
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
#include "OracleThreadPool.h"
#include "OracleServerAgent.h"
#include "OracleClient.h"
#include "OracleRequestBroadcastMessage.h"
#include "OracleResponseMessage.h"


OracleServerAgent::OracleServerAgent(Schain &_schain) : Agent(_schain, true), requestCounter(0), threadCounter(0) {

    for (int i = 0; i < NUM_ORACLE_THREADS; i++) {
        incomingQueues.push_back(
                make_shared<BlockingReaderWriterQueue<shared_ptr<MessageEnvelope>>>());
    }

    try {
        LOG(info, "Constructing OracleThreadPool");

        this->oracleThreadPool = make_shared<OracleThreadPool>(this);
        oracleThreadPool->startService();
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }


};

void OracleServerAgent::routeAndProcessMessage(const ptr<MessageEnvelope> &_me) {


    CHECK_ARGUMENT(_me);

    CHECK_ARGUMENT(_me->getMessage()->getBlockId() > 0);

    CHECK_STATE(_me->getMessage()->getMessageType() == MSG_ORACLE_REQ_BROADCAST ||
                _me->getMessage()->getMessageType() == MSG_ORACLE_RSP);

    if (_me->getMessage()->getMessageType() == MSG_ORACLE_REQ_BROADCAST) {

        auto value = requestCounter.fetch_add(1);

        this->incomingQueues.at(value % (uint64_t) NUM_ORACLE_THREADS)->enqueue(_me);

        return;
    } else {
        auto client = getSchain()->getOracleClient();
        client->processResponseMessage(_me);
    }

}

void OracleServerAgent::workerThreadItemSendLoop(OracleServerAgent *_agent) {

    CHECK_STATE(_agent)

    CHECK_STATE(_agent->threadCounter == 0);

    LOG(info, "Thread counter is : " + to_string(_agent->threadCounter));

    auto threadNumber = ++(_agent->threadCounter);

    LOG(info, "Starting Oracle worker thread: " + to_string(threadNumber));

    _agent->waitOnGlobalStartBarrier();

    LOG(info, "Started Oracle worker thread " + to_string(threadNumber));

    auto agent = (Agent *) _agent;

    try {
        while (!agent->getSchain()->getNode()->isExitRequested()) {

            ptr<MessageEnvelope> msge;

            auto success = _agent->incomingQueues.at(threadNumber - 1)->wait_dequeue_timed(msge,
                                                                                           1000 *
                                                                                           ORACLE_QUEUE_TIMEOUT_MS);
            if (!success)
                continue;

            auto orclMsg = dynamic_pointer_cast<OracleRequestBroadcastMessage>(msge->getMessage());

            CHECK_STATE(orclMsg);

            auto msg = _agent->doEndpointRequestResponse(orclMsg);

            _agent->sendOutResult(msg, msge->getSrcSchainIndex());

        }
    } catch (FatalError &e) {
        SkaleException::logNested(e);
        agent->getNode()->exitOnFatalError(e.what());
    } catch (ExitRequestedException &e) {
    } catch (exception &e) {
        SkaleException::logNested(e);
    }

    LOG(info, "Exited Oracle worker thread " + to_string(threadNumber));

}


ptr<OracleResponseMessage> OracleServerAgent::doEndpointRequestResponse(ptr<OracleRequestBroadcastMessage> _request) {
    CHECK_ARGUMENT(_request)




    string result = "{\"huhu\":\"huhu\"}";
    string receipt = _request->getHash().toHex();

    return make_shared<OracleResponseMessage>(result,
                                              receipt,
                                              getSchain()->getLastCommittedBlockID() + 1,
                                              Time::getCurrentTimeMs(),
                                              *getSchain()->getOracleClient());
}

void OracleServerAgent::sendOutResult(ptr<OracleResponseMessage> _msg, schain_index _destination) {
    CHECK_STATE(_destination != 0)

    getSchain()->getNode()->getNetwork()->sendOracleResponseMessage(_msg, _destination);

}