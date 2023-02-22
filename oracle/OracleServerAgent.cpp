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

#include <curl/curl.h>

#include "thirdparty/LUrlParser.h"

#include "SkaleCommon.h"
#include "Log.h"

#include "blockfinalize/client/BlockFinalizeDownloader.h"

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

#include "messages/ConsensusProposalMessage.h"
#include "messages/InternalMessageEnvelope.h"

#include "messages/NetworkMessageEnvelope.h"
#include "messages/ParentMessage.h"
#include "network/Network.h"

#include "node/NodeInfo.h"
#include "third_party/json.hpp"

#include "utils/Time.h"
#include "protocols/ProtocolInstance.h"
#include "OracleErrors.h"
#include "OracleRequestSpec.h"
#include "OracleThreadPool.h"
#include "OracleClient.h"
#include "OracleRequestBroadcastMessage.h"
#include "OracleResponseMessage.h"
#include "OracleResult.h"
#include "OracleServerAgent.h"

OracleServerAgent::OracleServerAgent(Schain &_schain) : Agent(_schain, true), requestCounter(0), threadCounter(0) {

    gethURL = getSchain()->getNode()->getGethUrl();

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
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }


};

void OracleServerAgent::routeAndProcessMessage(const ptr<MessageEnvelope> &_me) {


    try {

        CHECK_ARGUMENT(_me);

        CHECK_ARGUMENT(_me->getMessage()->getBlockId() > 0);

        CHECK_STATE(_me->getMessage()->getMsgType() == MSG_ORACLE_REQ_BROADCAST ||
                    _me->getMessage()->getMsgType() == MSG_ORACLE_RSP);

        if (_me->getMessage()->getMsgType() == MSG_ORACLE_REQ_BROADCAST) {

            auto value = requestCounter.fetch_add(1);

            this->incomingQueues.at(value % (uint64_t) NUM_ORACLE_THREADS)->enqueue(_me);

            return;
        } else {
            auto client = getSchain()->getOracleClient();
            client->processResponseMessage(_me);
        }

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
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

    while (!agent->getSchain()->getNode()->isExitRequested()) {
        try {

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
        } catch (ExitRequestedException &e) {
        } catch (exception &e) {
            SkaleException::logNested(e);
        } catch (...) {
            LOG(err, "Error in Oracle loop, unknown object is thrown");
        }
    }

    LOG(info, "Exited Oracle worker thread " + to_string(threadNumber));

}

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *) userp;

    char *ptr = (char *) realloc(mem->memory, mem->size + realsize + 1);
    CHECK_STATE(ptr);
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

using namespace nlohmann;

ptr<OracleResponseMessage> OracleServerAgent::doEndpointRequestResponse(ptr<OracleRequestBroadcastMessage> _request) {
    CHECK_ARGUMENT(_request)

    auto spec = _request->getParsedSpec();

    auto uri = spec->getUri();
    if (spec->isEthMainnet()) {
        uri = gethURL + "/" + spec->getEthApi();
    } else {
        auto result = LUrlParser::ParseURL::parseURL(uri);
        CHECK_STATE2(result.isValid(), "URL invalid:" + uri);
        CHECK_STATE2(result.userName_.empty(), "Non empty username");
        CHECK_STATE2(result.password_.empty(), "Non empty password");
        auto host = result.host_;

        CHECK_STATE2(host.find("0.") != 0 &&
                     host.find("10.") != 0 &&
                     host.find("127.") != 0 &&
                     host.find("172.") != 0 &&
                     host.find("192.168.") != 0 &&
                     host.find("169.254.") != 0 &&
                     host.find("192.0.0") != 0 &&
                     host.find("192.0.2") != 0 &&
                     host.find("192.0.2") != 0 &&
                     host.find("198.18") != 0 &&
                     host.find("198.19") != 0,
                     "Private IPs not allowed in Oracle"
        )
    }

    auto postString = spec->getPost();

    string response;


    auto status = curlHttp(uri, spec->isPost(), postString, response);

    ptr<OracleResult> oracleResult = nullptr;

    oracleResult = make_shared<OracleResult>(spec, status, response, getSchain()->getCryptoManager());

    auto resultStr = oracleResult->toString();

    LOG(info, "Oracle request result: " + resultStr);

    string receipt = _request->getParsedSpec()->getReceipt();

    return make_shared<OracleResponseMessage>(resultStr,
                                              receipt,
                                              getSchain()->getLastCommittedBlockID() + 1,
                                              Time::getCurrentTimeMs(),
                                              *getSchain()->getOracleClient());
}


uint64_t OracleServerAgent::curlHttp(const string &_uri, bool _isPost, string &_postString, string &_result) {


    uint64_t status = ORACLE_UNKNOWN_ERROR;
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = (char *) malloc(1);  /* will be grown as needed by the realloc above */
    CHECK_STATE(chunk.memory);
    chunk.size = 0;    /* no data at this point */

    curl = curl_easy_init();

    CHECK_STATE2(curl, "Could not init curl object");

    curl_easy_setopt(curl, CURLOPT_URL, _uri.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    string pagedata;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &chunk);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl, CURLOPT_DNS_SERVERS, "8.8.8.8");

    if (_isPost) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, _postString.c_str());
    }

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        LOG(err, "Curl easy perform failed for url: " + _uri + " with error code:" + to_string(res));
        status = ORACLE_COULD_NOT_CONNECT_TO_ENDPOINT;
    } else {
        status = ORACLE_SUCCESS;
    }

    curl_easy_cleanup(curl);

    string r = string(chunk.memory, chunk.size);

    free(chunk.memory);

    _result = r;

    return status;
}

void OracleServerAgent::sendOutResult(ptr<OracleResponseMessage> _msg, schain_index _destination) {

    try {

        CHECK_STATE(_destination != 0)

        getSchain()->getNode()->getNetwork()->sendOracleResponseMessage(_msg, _destination);


    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

