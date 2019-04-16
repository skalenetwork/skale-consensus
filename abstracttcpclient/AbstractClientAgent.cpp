//
// Created by kladko on 3/25/19.
//

#include "../../SkaleConfig.h"
#include "../../Log.h"
#include "../thirdparty/json.hpp"

#include "../../abstracttcpclient/AbstractClientAgent.h"
#include "../../node/Node.h"
#include "../../Agent.h"
#include "../network/ClientSocket.h"
#include "../network/IO.h"
#include "../exceptions/Exception.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/ExitRequestedException.h"
#include "../exceptions/NetworkProtocolException.h"

#include "../SkaleConfig.h"
#include "../chains/Schain.h"
#include "../thirdparty/json.hpp"
#include "../node/Node.h"
#include "../node/NodeInfo.h"
#include "../crypto/SHAHash.h"


#include "../datastructures/BlockProposal.h"

#include "../thirdparty/json.hpp"

#include "AbstractClientAgent.h"



AbstractClientAgent::AbstractClientAgent(Schain &_sChain, port_type _portType) : Agent(_sChain, false){

    portType = _portType;


    logThreadLocal_ = _sChain.getNode()->getLog();

    for (uint64_t i = 0; i < _sChain.getNodeCount(); i++) {
        (itemQueue)
                .emplace(schain_index(i), make_shared<queue<ptr<BlockProposal> > >());
        (queueCond).emplace(schain_index(i), make_shared<condition_variable>());
        (queueMutex).emplace(schain_index(i), make_shared<mutex>());

        ASSERT(itemQueue.count(schain_index(i)));
        ASSERT(queueCond.count(schain_index(i)));
        ASSERT(queueMutex.count(schain_index(i)));
    }

    threadCounter = 0;

}

uint64_t AbstractClientAgent::incrementAndReturnThreadCounter() {
    return threadCounter++; // l_sergiy: increment/return-prev of atomic variable
}



void AbstractClientAgent::sendItem(ptr<BlockProposal> _proposal, schain_index _dstIndex, node_id _dstNodeId) {



    ASSERT(getNode()->isStarted());

    auto socket = make_shared<ClientSocket>(*sChain, _dstIndex, portType);



    try {
        getSchain()->getIo()->writeMagic(socket);
    }

    catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(NetworkProtocolException("Could not write magic", __CLASS_NAME__));
    }


    sendItemImpl(_proposal, socket, _dstIndex, _dstNodeId);
}




void AbstractClientAgent::enqueueItem(ptr<BlockProposal> item) {
    for (uint64_t i = 0; i < (uint64_t) this->sChain->getNodeCount(); i++) {
        {
            lock_guard<mutex> lock(*queueMutex[schain_index(i)]);
            auto q = itemQueue[schain_index(i)];
            q->push(item);
        }
        queueCond[schain_index(i)]->notify_all();
    }
}





void AbstractClientAgent::workerThreadItemSendLoop(AbstractClientAgent *agent) {
    setThreadName(__CLASS_NAME__.substr(0, 15));

    agent->waitOnGlobalStartBarrier();

    auto destinationSubChainIndex = schain_index(agent->incrementAndReturnThreadCounter());

    try {


        while (!agent->getSchain()->getNode()->isExitRequested()) {

            {
                std::unique_lock<std::mutex> mlock(*agent->queueMutex[destinationSubChainIndex]);


                while (agent->itemQueue[destinationSubChainIndex]->empty()) {
                    agent->getSchain()->getNode()->exitCheck();
                    agent->queueCond[destinationSubChainIndex]->wait(mlock);
                }
            }

            ASSERT(agent->itemQueue[destinationSubChainIndex]);

            auto proposal = agent->itemQueue[destinationSubChainIndex]->front();


            ASSERT(proposal);

            agent->itemQueue[destinationSubChainIndex]->pop();



            if (destinationSubChainIndex != agent->getSchain()->getSchainIndex()) {

                auto nodeId = agent->getSchain()->getNode()->getNodeInfoByIndex(destinationSubChainIndex)->getNodeID();

                bool sent = false;




                while (!sent) {
                    try {
                        agent->sendItem(proposal, destinationSubChainIndex, nodeId);
                        sent = true;
                    } catch (Exception &e) {
                        Exception::log_exception(e);
                        if (agent->getNode()->isExitRequested())
                            return;
                        usleep(agent->getNode()->getWaitAfterNetworkErrorMs() * 1000);
                    }
                }
            }
        };
    }

    catch (FatalError *e) {
        agent->getNode()->exitOnFatalError(e->getMessage());
    }
    catch (ExitRequestedException &e) {
        return;
    }
}