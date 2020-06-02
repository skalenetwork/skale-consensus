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

    @file TransportNetwork.cpp
    @author Stan Kladko
    @date 2018
*/


#include "SkaleCommon.h"
#include "SkaleLog.h"
#include "abstracttcpserver/ConnectionStatus.h"
#include "blockproposal/pusher/BlockProposalClientAgent.h"
#include "blockproposal/server/BlockProposalWorkerThreadPool.h"
#include "chains/Schain.h"
#include "crypto/ConsensusBLSSigShare.h"
#include "crypto/SHAHash.h"
#include "datastructures/BlockProposal.h"
#include "db/BlockProposalDB.h"
#include "exceptions/FatalError.h"
#include "messages/NetworkMessage.h"
#include "node/Node.h"
#include "node/NodeInfo.h"
#include "protocols/binconsensus/AUXBroadcastMessage.h"
#include "protocols/binconsensus/BVBroadcastMessage.h"
#include "protocols/blockconsensus/BlockSignBroadcastMessage.h"
#include "thirdparty/json.hpp"
#include <db/MsgDB.h>

#include "unordered_set"


#include "exceptions/ExitRequestedException.h"
#include "exceptions/InvalidMessageFormatException.h"
#include "exceptions/InvalidSchainException.h"
#include "exceptions/InvalidSourceIPException.h"
#include "messages/Message.h"
#include "protocols/blockconsensus/BlockConsensusAgent.h"

#include "Buffer.h"
#include "Network.h"
#include "messages/NetworkMessageEnvelope.h"
#include "network/Sockets.h"
#include "network/ZMQSockets.h"
#include "threads/GlobalThreadRegistry.h"

TransportType Network::transport = TransportType::ZMQ;


void Network::addToDeferredMessageQueue(ptr<NetworkMessageEnvelope> _me) {
    CHECK_ARGUMENT(_me);

    auto msg = dynamic_pointer_cast<NetworkMessage>(_me->getMessage());



    auto _blockID = _me->getMessage()->getBlockID();

    ptr<vector<ptr<NetworkMessageEnvelope> > > messageList;

    {
        lock_guard<recursive_mutex> l(deferredMessageMutex);

        if (deferredMessageQueue.count(_blockID) == 0) {
            messageList = make_shared<vector<ptr<NetworkMessageEnvelope>>>();
            deferredMessageQueue[_blockID] = messageList;
        } else {
            messageList = deferredMessageQueue[_blockID];
        };

        messageList->push_back(_me);
    }
}

ptr<vector<ptr<NetworkMessageEnvelope> > > Network::pullMessagesForCurrentBlockID() {


    LOCK(deferredMessageMutex);


    block_id currentBlockID = sChain->getLastCommittedBlockID() + 1;


    auto returnList = make_shared<vector<ptr<NetworkMessageEnvelope>>>();


    for (auto it = deferredMessageQueue.cbegin();
         it != deferredMessageQueue.cend() /* not hoisted */;
        /* no increment */ ) {
        if (it->first <= currentBlockID) {
            for (auto &&msg : *(it->second)) {
                returnList->push_back(msg);
            }

            it = deferredMessageQueue.erase(it);
        } else {
            ++it;
        }
    }

    return returnList;
}

void Network::addToDelayedSends(ptr<NetworkMessage> _m, ptr<NodeInfo> dstNodeInfo) {
    CHECK_ARGUMENT(_m);
    CHECK_ARGUMENT(dstNodeInfo);
    auto dstIndex = (uint64_t) dstNodeInfo->getSchainIndex();
    LOCK(delayedSendsLocks.at(dstIndex - 1));
    delayedSends.at(dstIndex - 1).push_back({_m, dstNodeInfo});
    if (delayedSends.at(dstIndex - 1).size() > MAX_DELAYED_MESSAGE_SENDS) {
        delayedSends.at(dstIndex - 1).pop_front();
    }
}

void Network::broadcastMessage(ptr<NetworkMessage> _m) {


    if (_m->getBlockID() <= this->catchupBlocks) {
        return;
    }

    try {



        // sign message before sending
        _m->sign(getSchain()->getCryptoManager());

        getSchain()->getNode()->getOutgoingMsgDB()->saveMsg(_m);

        unordered_set<uint64_t> sent;

        // wait until we send to at least 2/3 of participants
        while (3 * (sent.size() + 1) < getSchain()->getNodeCount() * 2) {
            for (auto const &it : *getSchain()->getNode()->getNodeInfosByIndex()) {
                auto dstNodeInfo = it.second;
                auto dstIndex = (uint64_t) dstNodeInfo->getSchainIndex();

                if (dstIndex != (getSchain()->getSchainIndex()) && !sent.count(dstIndex)) {
                    if (sendMessage(it.second, _m)) {
                        sent.insert(dstIndex);
                    }
                }
            }
        }

        // messages that could not be sent because the receiving nodes were not online are
        // queued to delayed sends to be tried later. The delayed sends queue for
        // each destination can have MAX_DELAYED_MESSAGE_SENDS

        for (auto const &it : *getSchain()->getNode()->getNodeInfosByIndex()) {
            auto dstNodeInfo = it.second;
            auto dstIndex = (uint64_t) dstNodeInfo->getSchainIndex();
            if (dstIndex != (getSchain()->getSchainIndex()) && !sent.count(dstIndex)) {
                addToDelayedSends(_m, dstNodeInfo);
            }
        }

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

void Network::networkReadLoop() {
    setThreadName("NtwkRdLoop", getSchain()->getNode()->getConsensusEngine());
    waitOnGlobalStartBarrier();

    try {
        while (!sChain->getNode()->isExitRequested()) {
            try {
                ptr<NetworkMessageEnvelope> m = receiveMessage();

                if (!m)
                    continue;  // check exit again

                if (m->getMessage()->getBlockID() <= catchupBlocks) {
                    continue;
                }

                ASSERT(sChain);

                getSchain()->getNode()->getIncomingMsgDB()->saveMsg(dynamic_pointer_cast<NetworkMessage>(m->getMessage()));

                postDeferOrDrop(m);
            } catch (ExitRequestedException &) {
                return;
            } catch (FatalError &) {
                throw;
            } catch (exception &e) {
                if (sChain->getNode()->isExitRequested()) {
                    sChain->getNode()->getSockets()->consensusZMQSockets->closeReceive();
                    return;
                }
                SkaleException::logNested(e);
            }

        }  // while
    } catch (FatalError &e) {
        sChain->getNode()->exitOnFatalError(e.getMessage());
    }

    sChain->getNode()->getSockets()->consensusZMQSockets->closeReceive();
}


/*
 * Consensus initially defers messages that come from the "future" - those that
 * have the block_id or the consensus round larger than currently processed.
 * These messages are placed in deferredMessage queue to be processed later.
 * Messages with very old block ids are discarded.
 */
void Network::postDeferOrDrop(const ptr<NetworkMessageEnvelope> &m) {

    block_id currentBlockID = sChain->getLastCommittedBlockID() + 1;


    auto bid = m->getMessage()->getBlockID();

    if (bid > currentBlockID) {
        // block id is in the future, defer
        addToDeferredMessageQueue(m);
        return;
    }

    if (bid + MAX_ACTIVE_CONSENSUSES <= currentBlockID) {
        // too old, drop
        return;
    }

    auto msg = dynamic_pointer_cast<NetworkMessage>(m->getMessage());

    CHECK_STATE(msg);

    // ask consensus whether to defer

    if (sChain->getBlockConsensusInstance()->shouldPost(msg)) {
        sChain->postMessage(m);
    } else {
        addToDeferredMessageQueue(m);
    }

}

void Network::trySendingDelayedSends() {
    auto nodeCount = getSchain()->getNodeCount();
    auto schainIndex = getSchain()->getSchainIndex();

    for (int i = 0; i < nodeCount; i++) {
        if (i != (schainIndex - 1)) {

            ptr<NetworkMessage> msg = nullptr;
            ptr<NodeInfo> dstNodeInfo = nullptr;

            while (true) {
                {
                    LOCK( delayedSendsLocks.at( i ) );

                    if ( delayedSends.at( i ).size() == 0 ) {
                        break;
                    }
                    dstNodeInfo = delayedSends.at( i ).front().second;
                    msg = delayedSends.at( i ).front().first;
                }
                if (sendMessage(dstNodeInfo, msg)) {
                    // successfully sent a delayed message, remove it from the list
                    {
                        LOCK( delayedSendsLocks.at( i ));
                        delayedSends.at( i ).pop_front();
                    }
                } {
                    // could not send a message to this host, no point trying to
                    // send other delayed messages for this host
                    break;
                }

            }
        }
    }
}

void Network::deferredMessagesLoop() {
    setThreadName("DeferMsgLoop", getSchain()->getNode()->getConsensusEngine());

    waitOnGlobalStartBarrier();

    while (!getSchain()->getNode()->isExitRequested()) {
        try {
            ptr<vector<ptr<NetworkMessageEnvelope> > > deferredMessages;

            // Get messages for the current block id
            deferredMessages = pullMessagesForCurrentBlockID();

            for (auto message : *deferredMessages) {
                postDeferOrDrop(message);
            }

            trySendingDelayedSends();
        }
        catch (ExitRequestedException &) {
            // exit
            LOG(info, "Exit requested, exiting deferred messages loop");
            return;
        } catch (Exception &e) {
            // print the error and continue the loop
            SkaleException::logNested(e);
        }
        usleep(100000);
    }
}


void Network::startThreads() {
    networkReadThread =
            make_shared<thread>(std::bind(&Network::networkReadLoop, this));
    deferredMessageThread =
            make_shared<thread>(std::bind(&Network::deferredMessagesLoop, this));

    auto reg = getSchain()->getNode()->getConsensusEngine()->getThreadRegistry();

    reg->add(networkReadThread);
    reg->add(deferredMessageThread);
}

bool Network::validateIpAddress(ptr<string> &ip) {
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ip.get()->c_str(), &(sa.sin_addr));
    return result != 0;
}


void Network::waitUntilExit() {
    networkReadThread->join();
    deferredMessageThread->join();
}

ptr<string> Network::ipToString(uint32_t _ip) {
    char *ip = (char *) &_ip;
    return make_shared<string>(
            to_string((uint8_t) ip[0]) + "." + to_string((uint8_t) ip[1]) + "." +
            to_string((uint8_t) ip[2]) + "." + to_string((uint8_t) ip[3]));
}

ptr<NetworkMessageEnvelope> Network::receiveMessage() {
    auto buf = make_shared<Buffer>(MAX_CONSENSUS_MESSAGE_LEN);
    uint64_t readBytes = readMessageFromNetwork(buf);

    auto msg = make_shared<string>((const char *) buf->getBuf()->data(), readBytes);

    auto mptr = NetworkMessage::parseMessage(msg, getSchain());

    mptr->verify(getSchain()->getCryptoManager());

    ptr<NodeInfo> realSender = sChain->getNode()->getNodeInfoByIndex(mptr->getSrcSchainIndex());

    if (realSender == nullptr) {
        BOOST_THROW_EXCEPTION(InvalidStateException("NetworkMessage from unknown sender schain index",
                                                    __CLASS_NAME__));
    }

    ptr<ProtocolKey> key = mptr->createDestinationProtocolKey();

    if (key == nullptr) {
        BOOST_THROW_EXCEPTION(InvalidMessageFormatException(
                                      "Network Message with corrupt protocol key", __CLASS_NAME__ ));
    };

    return make_shared<NetworkMessageEnvelope>(mptr, realSender);
};


void Network::setTransport(TransportType _transport) {
    Network::transport = _transport;
}

TransportType Network::getTransport() {
    return transport;
}

uint32_t Network::getPacketLoss() const {
    return packetLoss;
}

void Network::setPacketLoss(uint32_t _packetLoss) {
    Network::packetLoss = _packetLoss;
}

void Network::setCatchupBlocks(uint64_t _catchupBlocks) {
    Network::catchupBlocks = _catchupBlocks;
}

uint64_t Network::getCatchupBlock() const {
    return catchupBlocks;
}

uint64_t Network::computeTotalDelayedSends() {
    uint64_t total = 0;
    for (uint64_t i = 0; i < delayedSends.size(); i++) {
        {   LOCK(delayedSendsLocks.at(i))
            total += delayedSends.at(i).size();
        }
    }
    return total;
}

Network::Network(Schain &_sChain)
        : Agent(_sChain, false),
      delayedSendsLocks((uint64_t) _sChain.getNodeCount()),
      delayedSends((uint64_t) _sChain.getNodeCount()) {
    auto cfg = _sChain.getNode()->getCfg();



    if (cfg.find("catchupBlocks") != cfg.end()) {
        uint64_t catchupBlock = cfg.at("catchupBlocks").get<uint64_t>();
        setCatchupBlocks(catchupBlock);
    }

    if (cfg.find("packetLoss") != cfg.end()) {
        uint32_t pl = cfg.at("packetLoss").get<uint64_t>();
        ASSERT(pl <= 100);
        setPacketLoss(pl);
    }
}

Network::~Network() {
}
