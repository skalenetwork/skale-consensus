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

    @file OracleServerAgent.h
    @author Stan Kladko
    @date 2021-
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
class OracleThreadPool;
class OracleRequestSpec;

class OracleServerAgent : public Agent {

    vector<shared_ptr<BlockingReaderWriterQueue<shared_ptr<MessageEnvelope>>>> incomingQueues;

    atomic<uint64_t> requestCounter;

    atomic<uint64_t> threadCounter;

    ptr <OracleThreadPool> oracleThreadPool = nullptr;

    string gethURL;

    ptr<OracleResponseMessage> doEndpointRequestResponse(ptr<OracleRequestBroadcastMessage> /* _request */);

    void sendOutResult(ptr<OracleResponseMessage> _msg, schain_index /* _destination*/);



    uint64_t curlHttp(const string &_uri, bool _isPost, string& _postString, string &_result);

    ptr<vector<ptr<string>>> extractResults(
            string& _response, vector<string> & _jsps) const;

    void trimResults(ptr<vector<ptr<string>>> &_results, vector<uint64_t> &_trims) const;

    void appendResultsToSpec(string &_specStr, ptr<vector<ptr<string>>> &_results) const;

    void appendStatusToSpec(string &specStr, uint64_t _error) const;

    void buildAndSignResult(string &_result, ptr<vector<uint8_t>> _abiEncodedResult);



public:


    void routeAndProcessMessage(const ptr<MessageEnvelope>& _me );


    OracleServerAgent(Schain& _schain);

    virtual ~OracleServerAgent() {};

    static void workerThreadItemSendLoop(OracleServerAgent* _agent );

    void doCurlRequestResponse(ptr<OracleRequestSpec> _spec, string &_response, uint64_t &_status);

    ptr<vector<uint8_t>>
    abiEncodeResult(ptr<OracleRequestSpec> _spec, uint64_t _status, ptr<vector<ptr<string>>> _results);
};

