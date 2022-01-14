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
#include "third_party/json.hpp"

#include "utils/Time.h"
#include "protocols/ProtocolInstance.h"
#include "OracleRequestSpec.h"
#include "OracleThreadPool.h"

#include "OracleClient.h"
#include "OracleRequestBroadcastMessage.h"
#include "OracleResponseMessage.h"
#include "OracleErrors.h"
#include "OracleServerAgent.h"

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

    string response;

    auto resultStr = _request->getRequestSpec();

    auto status = curlHttpGet(spec->getUri(), response);

    if (status != ORACLE_SUCCESS) {
        appendErrorToSpec(resultStr, status);
    } else {
        auto jsps = spec->getJsps();
        auto results = extractResults(response, jsps);

        if (!results) {
            appendErrorToSpec(resultStr, ORACLE_INVALID_JSON_RESPONSE);
        } else {
            auto trims = spec->getTrims();
            trimResults(results, trims);
            appendResultsToSpec(resultStr, results);
        }
    }

    this->signResult(resultStr);

    cerr << resultStr << endl;



    string receipt = _request->getHash().toHex();

    return make_shared<OracleResponseMessage>(resultStr,
                                              receipt,
                                              getSchain()->getLastCommittedBlockID() + 1,
                                              Time::getCurrentTimeMs(),
                                              *getSchain()->getOracleClient());
}

void OracleServerAgent::appendResultsToSpec(string &specStr, ptr<vector<ptr<string>>> &_results) const {
    {

        auto commaPosition = specStr.find_last_of(",");

        CHECK_STATE(commaPosition != string::npos);

        specStr = specStr.substr(0, commaPosition + 1);

        specStr.append("\"rslts\":[");

        for (uint64_t i = 0; i < _results->size(); i++) {
            if (i != 0) {
                specStr.append(",");
            }

            if (_results->at(i)) {
                specStr.append("\"");
                specStr.append(*_results->at(i));
                specStr.append("\"");
            } else {
                specStr.append("null");
            }

        }

        specStr.append("],");
    }
}

void OracleServerAgent::appendErrorToSpec(string &specStr, uint64_t _error) const {
    auto commaPosition = specStr.find_last_of(",");
    CHECK_STATE(commaPosition != string::npos);
    specStr = specStr.substr(0, commaPosition + 1);
    specStr.append("\"err\":\"");
    specStr.append(to_string(_error));
    specStr.append("\",");
}


void OracleServerAgent::trimResults(ptr<vector<ptr<string>>> &_results, vector<uint64_t> &_trims) const {

    CHECK_STATE(_results->size() == _trims.size())

    for (uint64_t i = 0; i < _results->size(); i++) {
        auto trim = _trims.at(i);
        auto res = _results->at(i);
        if (res && trim != 0) {
            if (res->size() <= trim) {
                res = make_shared<string>("");
            } else {
                res = make_shared<string>(res->substr(0, res->size() - trim));
            }
            (*_results)[i] = res;
        }

    }
}

ptr<vector<ptr<string>>> OracleServerAgent::extractResults(
        string &_response,
        vector<string> &jsps) const {


    auto rs = make_shared<vector<ptr<string>>>();


    try {

        auto j = json::parse(_response);
        for (auto &&jsp: jsps) {
            auto pointer = json::json_pointer(jsp);
            try {
                auto val = j.at(pointer);
                CHECK_STATE(val.is_primitive());
                string strVal;
                if (val.is_string()) {
                    strVal = val.get<string>();
                } else if (val.is_number_integer()) {
                    if (val.is_number_unsigned()) {
                        strVal = to_string(val.get<uint64_t>());
                    } else {
                        strVal = to_string(val.get<int64_t>());
                    }
                } else if (val.is_number_float()) {
                    strVal = to_string(val.get<double>());
                } else if (val.is_boolean()) {
                    strVal = to_string(val.get<bool>());
                }
                rs->push_back(make_shared<string>(strVal));
            } catch (...) {
                rs->push_back(nullptr);
            }
        }

    } catch (...) {
        return nullptr;
    }

    return rs;
}

uint64_t OracleServerAgent::curlHttpGet(const string &_uri, string &_result) {
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
    CHECK_STATE(_destination != 0)

    getSchain()->getNode()->getNetwork()->sendOracleResponseMessage(_msg, _destination);

}

void OracleServerAgent::signResult(string & _result) {
    _result.append("\"sig\":\"");
    _result.append("0x012345678");
    _result.append("\"}");
}
