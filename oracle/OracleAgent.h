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

    @file OracleAgent.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

class Schain;

class OracleRequestBroadcastMessage;
class CryptoManager;


#include "protocols/ProtocolInstance.h"


#include "thirdparty/lrucache.hpp"
#include "sgxwallet/third_party/concurrentqueue.h"
#include "sgxwallet/third_party/readerwriterqueue.h"

using namespace moodycamel;

class MessageEnvelope;
class OracleResponseMessage;
class OracleRequestBroadcastMessage;

class OracleAgent : public ProtocolInstance {

    recursive_mutex m;

    ConcurrentQueue<shared_ptr<MessageEnvelope>> outgoingQueue;

    vector<BlockingReaderWriterQueue<shared_ptr<MessageEnvelope>>> incomingQueues;

public:


    OracleAgent(Schain& _schain);


    void routeAndProcessMessage(const ptr<MessageEnvelope>& _me );

    void doOracle(const ptr<OracleRequestBroadcastMessage> &_m);
};

