/*
    Copyright (C) 2018- SKALE Labs

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
    @date 2018-
*/


#include "Log.h"
#include "SkaleCommon.h"
#include "blockproposal/pusher/BlockProposalClientAgent.h"
#include "chains/Schain.h"
#include "crypto/BLAKE3Hash.h"
#include "crypto/ConsensusBLSSigShare.h"
#include "datastructures/BlockProposal.h"
#include "db/BlockProposalDB.h"
#include "exceptions/FatalError.h"
#include "messages/NetworkMessage.h"
#include "oracle/OracleRequestBroadcastMessage.h"
#include "oracle/OracleResponseMessage.h"
#include "node/Node.h"
#include "node/NodeInfo.h"
#include "protocols/blockconsensus/BlockSignBroadcastMessage.h"
#include "thirdparty/json.hpp"
#include "thirdparty/lrucache.hpp"
#include "oracle/OracleResultAssemblyAgent.h"
#include <db/MsgDB.h>

#include "unordered_set"


#include "exceptions/ExitRequestedException.h"
#include "exceptions/InvalidMessageFormatException.h"
#include "protocols/blockconsensus/BlockConsensusAgent.h"

#include "Buffer.h"
#include "Network.h"
#include "messages/NetworkMessageEnvelope.h"
#include "network/Sockets.h"
#include "network/ZMQSockets.h"
#include "utils/Time.h"
#include "threads/GlobalThreadRegistry.h"

TransportType Network::transport = TransportType::ZMQ;


void Network::addToDeferredMessageQueue(const ptr<NetworkMessageEnvelope> &_me) {
    CHECK_ARGUMENT(_me);

    auto msg = dynamic_pointer_cast<NetworkMessage>(_me->getMessage());


    auto _blockID = _me->getMessage()->getBlockID();

    ptr<list<ptr<NetworkMessageEnvelope> > > messageList;

    {
        LOCK(deferredMessageMutex);

        if (deferredMessageQueue.count(_blockID) == 0) {
            messageList = make_shared<list<ptr<NetworkMessageEnvelope> > >();
            deferredMessageQueue[_blockID] = messageList;
        } else {
            messageList = deferredMessageQueue[_blockID];
        };

        messageList->push_back(_me);

        if (messageList->size() > MAX_DEFERRED_QUEUE_SIZE_FOR_BLOCK)
            messageList->pop_front();
    }
}

ptr<vector<ptr<NetworkMessageEnvelope> > > Network::pullMessagesForCurrentBlockID() {
    block_id currentBlockID = sChain->getLastCommittedBlockID() + 1;

    auto returnList = make_shared<vector<ptr<NetworkMessageEnvelope> > >();

    LOCK(deferredMessageMutex);

    for (auto it = deferredMessageQueue.cbegin();
         it != deferredMessageQueue.cend() /* not hoisted */;
        /* no increment */ ) {
        if (it->first <= currentBlockID) {
            for (auto &&msg: *(it->second)) {
                returnList->push_back(msg);
            }

            it = deferredMessageQueue.erase(it);
        } else {
            ++it;
        }
    }

    return returnList;
}

void Network::addToDelayedSends(
        const ptr<NetworkMessage> &_m, const ptr<NodeInfo> &_dstNodeInfo) {
    CHECK_ARGUMENT(_m);
    CHECK_ARGUMENT(_dstNodeInfo);
    auto dstIndex = (uint64_t) _dstNodeInfo->getSchainIndex();
    LOCK(delayedSendsLocks.at(dstIndex - 1));
    delayedSends.at(dstIndex - 1).push_back({_m, _dstNodeInfo});
    if (delayedSends.at(dstIndex - 1).size() > MAX_DELAYED_MESSAGE_SENDS) {
        delayedSends.at(dstIndex - 1).pop_front();
    }
}

void Network::broadcastMessage(const ptr<NetworkMessage> &_msg) {
    broadcastMessageImpl(_msg, true);
}

void Network::rebroadcastMessage(const ptr<NetworkMessage> &_msg) {
    broadcastMessageImpl(_msg, false);
}

void Network::broadcastMessageImpl(const ptr<NetworkMessage> &_msg, bool _isFirstBroadcast) {
    CHECK_ARGUMENT(_msg);

    // used for testing
    if (_msg->getBlockID() <= this->catchupBlocks) {
        return;
    }


    if (_msg->getBlockID() == 5
        && getSchain()->getBlockProposerTest() == "BAD_NETWORK") {
        return;
    }

    try {
        if (_isFirstBroadcast) {
            // sign message before sending
            _msg->sign(getSchain()->getCryptoManager());
            getSchain()->getNode()->getOutgoingMsgDB()->saveMsg(_msg);
        }


        unordered_set<uint64_t> sent;

        // wait until we send to at least 2/3 of participants
        while (3 * (sent.size() + 1) < getSchain()->getNodeCount() * 2) {
            for (auto const &it: *getSchain()->getNode()->getNodeInfosByIndex()) {
                auto dstNodeInfo = it.second;
                auto dstIndex = (uint64_t) dstNodeInfo->getSchainIndex();

                if (dstIndex != (getSchain()->getSchainIndex()) && !sent.count(dstIndex)) {
                    if (sendMessage(it.second, _msg)) {
                        sent.insert(dstIndex);
                    }
                }
            }
            sleep(0);
        }

        // messages that could not be sent because the receiving nodes were not online are
        // queued to delayed sends to be tried later. The delayed sends queue for
        // each destination can have MAX_DELAYED_MESSAGE_SENDS.

        for (auto const &it: *getSchain()->getNode()->getNodeInfosByIndex()) {
            auto dstNodeInfo = it.second;
            CHECK_STATE(dstNodeInfo);
            auto dstIndex = (uint64_t) dstNodeInfo->getSchainIndex();
            if (dstIndex != (getSchain()->getSchainIndex()) && !sent.count(dstIndex)) {
                addToDelayedSends(_msg, dstNodeInfo);
            }
        }

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void Network::broadcastOracleRequestMessage(const ptr<OracleRequestBroadcastMessage> &_msg) {
    // Oracle messages are simply broadcast without resends
    CHECK_ARGUMENT(_msg);


    try {

        _msg->sign(getSchain()->getCryptoManager());

        for (auto const &it: *getSchain()->getNode()->getNodeInfosByIndex()) {
            auto dstNodeInfo = it.second;
            auto dstIndex = (uint64_t) dstNodeInfo->getSchainIndex();

            if (dstIndex != (getSchain()->getSchainIndex())) {
                sendMessage(it.second, _msg);
            } else {
                getSchain()->getOracleResultAssemblyAgent()->postMessage(
                        make_shared<NetworkMessageEnvelope>(_msg, dstIndex));
            }
        }
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

void Network::sendOracleResponseMessage(const ptr<OracleResponseMessage> &_msg, schain_index _dstIndex) {
    // Oracle messages are simply sent without resends
    CHECK_ARGUMENT(_msg);

    try {

        _msg->sign(getSchain()->getCryptoManager());


        if (_dstIndex != (getSchain()->getSchainIndex())) {
            auto dstNodeInfo = getSchain()->getNode()->getNodeInfoByIndex(_dstIndex);
            CHECK_STATE(dstNodeInfo);
            sendMessage(dstNodeInfo, _msg);
        } else {
            getSchain()->getOracleResultAssemblyAgent()->postMessage(make_shared<NetworkMessageEnvelope>(_msg, _dstIndex));
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

                auto msg = dynamic_pointer_cast<NetworkMessage>(m->getMessage());


                if (msg->getBlockID() <= catchupBlocks) {
                    continue;
                }


                if (!knownMsgHashes.putIfDoesNotExist(msg->getHash().toHex(), true)) {
                    // already seen this message, dropping
                    continue;
                }

                if (msg->getMsgType() == MSG_ORACLE_REQ_BROADCAST  || msg->getMsgType() == MSG_ORACLE_RSP) {
                    sChain->getOracleResultAssemblyAgent()->postMessage(m);
                    continue;
                }

                CHECK_STATE(sChain);

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
        SkaleException::logNested(e);
        sChain->getNode()->exitOnFatalError(e.what());
    }

    sChain->getNode()->getSockets()->consensusZMQSockets->closeReceive();
}


/*
 * Consensus initially defers messages that come from the "future" - those that
 * have the block_id or the consensus round larger than currently processed.
 * These messages are placed in deferredMessage queue to be processed later.
 * Messages with very old block ids are discarded.
 */


void Network::postDeferOrDrop(const ptr<NetworkMessageEnvelope> &_me) {
    CHECK_ARGUMENT(_me);

    block_id currentBlockID = sChain->getLastCommittedBlockID() + 1;

    auto bid = _me->getMessage()->getBlockID();


    if (bid > currentBlockID) {
        // block id is in the future, defer
        addToDeferredMessageQueue(_me);
        return;
    }

    if (bid + MAX_ACTIVE_CONSENSUSES <= currentBlockID) {
        // too old, drop
        return;
    }

    // ask consensus whether to defer

    auto msg = dynamic_pointer_cast<NetworkMessage>(_me->getMessage());

    CHECK_STATE(msg);

    if (msg->getMsgType() == MSG_ORACLE_REQ_BROADCAST || msg->getMsgType() == MSG_ORACLE_RSP) {
        sChain->getOracleResultAssemblyAgent()->postMessage(_me);
    } else if (sChain->getBlockConsensusInstance()->shouldPost(msg)) {
        sChain->postMessage(_me);
    } else {
        addToDeferredMessageQueue(_me);
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
                    LOCK(delayedSendsLocks.at(i));

                    if (delayedSends.at(i).size() == 0) {
                        break;
                    }
                    msg = delayedSends.at(i).front().first;
                    dstNodeInfo = delayedSends.at(i).front().second;
                    CHECK_STATE(dstNodeInfo);
                    CHECK_STATE(msg);
                }
                if (sendMessage(dstNodeInfo, msg)) {
                    // successfully sent a delayed message, remove it from the list
                    {
                        LOCK(delayedSendsLocks.at(i));
                        delayedSends.at(i).pop_front();
                    }
                }
                {
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

            CHECK_STATE(deferredMessages);

            for (auto message: *deferredMessages) {
                if (getSchain()->getNode()->isExitRequested())
                    return;
                postDeferOrDrop(message);
            }

            trySendingDelayedSends();
        } catch (ExitRequestedException &) {
            // exit
            LOG(info, "Exit requested, exiting deferred messages loop");
            return;
        } catch (SkaleException &e) {
            // print the error and continue the loop
            SkaleException::logNested(e);
        }
        usleep(1000000);
    }
}


void Network::startThreads() {
    networkReadThread = make_shared<thread>(std::bind(&Network::networkReadLoop, this));
    deferredMessageThread =
            make_shared<thread>(std::bind(&Network::deferredMessagesLoop, this));

    auto reg = getSchain()->getNode()->getConsensusEngine()->getThreadRegistry();

    reg->add(networkReadThread);
    reg->add(deferredMessageThread);
}

bool Network::validateIpAddress(const string &_ip) {
    CHECK_ARGUMENT(!_ip.empty())
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, _ip.c_str(), &(sa.sin_addr));
    return result != 0;
}

string Network::ipToString(uint32_t _ip) {
    char *ip = (char *) &_ip;
    return string(to_string((uint8_t) ip[0]) + "." + to_string((uint8_t) ip[1]) + "." +
                  to_string((uint8_t) ip[2]) + "." + to_string((uint8_t) ip[3]));
}

ptr<NetworkMessageEnvelope> Network::receiveMessage() {
    auto buf = make_shared<Buffer>(MAX_NETWORK_MESSAGE_LEN);

    uint64_t readBytes = readMessageFromNetwork(buf);

    string msg((const char *) buf->getBuf()->data(), readBytes);

    auto mptr = NetworkMessage::parseMessage(msg, getSchain());

    CHECK_STATE(mptr);

    if (getSchain()->getNode()->getVisualizationType() > 0) {
        saveToVisualization(mptr, getSchain()->getNode()->getVisualizationType());
    }


    mptr->verify(getSchain()->getCryptoManager());


    ptr<NodeInfo> realSender = sChain->getNode()->getNodeInfoByIndex(mptr->getSrcSchainIndex());

    if (realSender == nullptr) {
        BOOST_THROW_EXCEPTION(InvalidStateException(
                                      "NetworkMessage from unknown sender schain index", __CLASS_NAME__ ));
    }

    ptr<ProtocolKey> key = mptr->createProtocolKey();

    CHECK_STATE(key);

    if (key == nullptr) {
        BOOST_THROW_EXCEPTION(InvalidMessageFormatException(
                                      "Network Message with corrupt protocol key", __CLASS_NAME__ ));
    };

    return make_shared<NetworkMessageEnvelope>(mptr, realSender->getSchainIndex());
};


void Network::setTransport(TransportType _transport) {
    Network::transport = _transport;
}

TransportType Network::getTransport() {
    return transport;
}

void Network::setPacketLoss(uint32_t _packetLoss) {
    Network::packetLoss = _packetLoss;
}

void Network::setCatchupBlocks(uint64_t _catchupBlocks) {
    Network::catchupBlocks = _catchupBlocks;
}

uint64_t Network::computeTotalDelayedSends() {
    uint64_t total = 0;
    for (uint64_t i = 0; i < delayedSends.size(); i++) {
        {
            LOCK(delayedSendsLocks.at(i))
            total += delayedSends.at(i).size();
        }
    }
    return total;
}

Network::Network(Schain &_sChain)
        : Agent(_sChain, false),
          knownMsgHashes(KNOWN_MSG_HASHES_SIZE),
          delayedSends((uint64_t) _sChain.getNodeCount()),
          delayedSendsLocks((uint64_t) _sChain.getNodeCount()) {

    // no network objects needed for sync nodes
    CHECK_STATE(!getNode()->isSyncOnlyNode());


    auto cfg = _sChain.getNode()->getCfg();



    if (cfg.find("catchupBlocks") != cfg.end()) {
        uint64_t catchupBlock = cfg.at("catchupBlocks").get<uint64_t>();
        setCatchupBlocks(catchupBlock);
    }

    if (cfg.find("packetLoss") != cfg.end()) {
        uint32_t pl = cfg.at("packetLoss").get<uint64_t>();
        CHECK_STATE(pl <= 100);
        setPacketLoss(pl);
    }
}

Network::~Network() {}

void Network::saveToVisualization(ptr<NetworkMessage> _msg, uint64_t _visualizationType) {
    CHECK_STATE(_msg);

    uint64_t round = 0;
    uint8_t value = 0;

    if (_msg->getMsgType() != MSG_BLOCK_SIGN_BROADCAST) {
        round = (uint64_t) _msg->getRound();
        value = (uint8_t) _msg->getValue();
    }

    string info = string("{") +
                  "\"t\":" + to_string(_msg->getMsgType()) + "," +
                  "\"b\":" + to_string(_msg->getTimeMs() -
                                       getSchain()->getStartTimeMs()) + "," +
                  "\"f\":" + to_string(Time::getCurrentTimeMs()
                                       - getSchain()->getStartTimeMs()) + "," +
                  "\"s\":" + to_string(_msg->getSrcSchainIndex()) + "," +
                  "\"d\":" + to_string(getSchain()->getSchainIndex()) + "," +
                  "\"p\":" + to_string(_msg->getBlockProposerIndex()) + "," +
                  "\"v\":" + to_string(value) + "," +
                  "\"r\":" + to_string(round) + "," +
                  "\"i\":" + to_string(_msg->getBlockID()) +
                  "}\n";


    if (_visualizationType == 1 || (_msg->getBlockProposerIndex() == 2 && _msg->getBlockID() == 3)) {
        Schain::writeToVisualizationStream(info);
    }
}
