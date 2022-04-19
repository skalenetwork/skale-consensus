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

    @file OracleRequestBroadcastMessage.h
    @author Stan Kladko
    @date 2021-
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "chains/Schain.h"
#include "messages/MessageEnvelope.h"
#include "network/Network.h"
#include "OracleResponseMessage.h"
#include "utils/Time.h"
#include "protocols/ProtocolInstance.h"
#include "OracleRequestSpec.h"
#include "OracleResult.h"
#include "OracleReceivedResults.h"
#include "OracleErrors.h"
#include "OracleRequestBroadcastMessage.h"
#include "OracleClient.h"


OracleClient::OracleClient(Schain &_sChain) : ProtocolInstance(ORACLE, _sChain), sChain(&_sChain),
                                              receiptsMap(ORACLE_RECEIPTS_MAP_SIZE) {

    gethURL = getSchain()->getNode()->getGethUrl();

    if (gethURL.empty()) {
        LOG(err, "Consensus initialized with empty gethURL. Geth-related Oracle functions will be disabled");
    }
}

uint64_t OracleClient::broadcastRequestAndReturnReceipt(ptr<OracleRequestBroadcastMessage> _msg, string &_receipt) {

    CHECK_STATE(_msg)
    CHECK_STATE(sChain)
    auto r = _msg->getParsedSpec()->getReceipt();


    auto exists = receiptsMap.putIfDoesNotExist(r,
                                                make_shared<OracleReceivedResults>(getSchain()->getRequiredSigners(),
                                                                                   (uint64_t) getSchain()->getNodeCount()));

    if (!exists) {
        LOG(err, "Request exists:" + r);
        return ORACLE_DUPLICATE_REQUEST;
    }

    LOCK(m);

    sChain->getNode()->getNetwork()->broadcastOracleRequestMessage(_msg);
    _receipt = r;
    return ORACLE_SUCCESS;
}


void OracleClient::sendTestRequestGet() {

    string spec;
    string _receipt;

    for (uint64_t i = 0; i < 1000000000; i++) {


        string cid = "\"cid\":" +
                     to_string((uint64_t) getSchain()->getSchainID());
        string uri = "\"uri\":\"http://worldtimeapi.org/api/timezone/Europe/Kiev\"";
        string jsps = "\"jsps\":[\"/unixtime\", \"/day_of_year\", \"/xxx\"]";
        string trims = "\"trims\":[1,1,1]";
        string time = "\"time\":" + to_string(Time::getCurrentTimeMs());
        string pow = "\"pow\":" + to_string(i);

        spec = "{" + cid + "," + uri + "," + jsps + "," + trims + "," + time + ","
                + pow + "}";

        auto os = make_shared<OracleRequestSpec>(spec);

        if (os->verifyPow()) {
            break;
        }
    }


    auto status = submitOracleRequest(spec, _receipt);

    CHECK_STATE(status == ORACLE_SUCCESS);


    std::thread t([this, _receipt]() {
        while (true) {
            string result;
            string r = _receipt;
            sleep(3);
            auto st = checkOracleResult(r, result);
            cerr << "ORACLE_STATUS:" << st << endl;
            if (st == ORACLE_SUCCESS) {
                cerr << result << endl;
                return;
            }

            if (st != ORACLE_RESULT_NOT_READY) {
                return;
            }
        }
    });
    t.detach();
};

void OracleClient::sendTestRequestPost() {

    if (getSchain()->getSchainIndex() != 1) {
        return;
    }

    string _receipt;

    string cid = "\"cid\":" +
                 to_string((uint64_t) getSchain()->getSchainID());
    string uri = "\"uri\":\"https://reqres.in/api/users\"";
    string jsps = "\"jsps\":[\"/id\"]";
    string time = "\"time\":" + to_string(Time::getCurrentTimeMs());
    string pow = "\"pow\":" + string("\"0x0000\"");
    string post = "\"post\":\"haha\"";

    string spec = "{" + cid + "," + uri + "," + jsps + "," + time + "," + pow +
                  +"," + post + "}";

    auto status = submitOracleRequest(spec, _receipt);

    CHECK_STATE(status == ORACLE_SUCCESS);

    string result;
}


uint64_t OracleClient::submitOracleRequest(const string& _spec, string &_receipt) {

    auto index = _spec.find_last_of(",");

    CHECK_STATE2(index != string::npos, "No comma in request");

    auto end = _spec.substr(index);

    CHECK_STATE2(end.find_last_of("pow") != string::npos, "Request does not end with pow element");

    auto msg = make_shared<OracleRequestBroadcastMessage>(_spec, sChain->getLastCommittedBlockID(),
                                                          Time::getCurrentTimeMs(),
                                                          *sChain->getOracleClient());
    return broadcastRequestAndReturnReceipt(msg, _receipt);
}


void OracleClient::processResponseMessage(const ptr<MessageEnvelope> &_me) {
    CHECK_STATE(_me);
    auto msg = dynamic_pointer_cast<OracleResponseMessage>(_me->getMessage());

    CHECK_STATE(msg);

    auto origin = (uint64_t) _me->getSrcSchainIndex();

    CHECK_STATE(origin > 0 || origin <= getSchain()->getNodeCount());

    auto receipt = msg->getReceipt();
    auto abiEncodedResult = msg->getOracleResult()->getAbiEncodedResult();
    auto sig = msg->getOracleResult()->getSig();

    auto receivedResults = receiptsMap.getIfExists(receipt);

    if (!receivedResults.has_value()) {
        LOG(warn, "Received OracleResponseMessage with unknown receipt" + receipt);
        return;
    }

    auto receipts = std::any_cast<ptr<OracleReceivedResults>>(receivedResults);


    receipts->insertIfDoesntExist(origin, abiEncodedResult, sig);


    LOG(info, "Processing oracle message:" + to_string(origin));

    string r;

    // tryGettingOracleResult(receipt, r);

}


uint64_t OracleClient::checkOracleResult(const string &_receipt,
                                         string &_result) {
    auto oracleReceivedResults = receiptsMap.getIfExists(_receipt);

    if (!oracleReceivedResults.has_value()) {
        LOG(warn, "Received tryGettingOracleResult  with unknown receipt" + _receipt);
        return ORACLE_UNKNOWN_RECEIPT;
    }


    auto receipts = std::any_cast<ptr<OracleReceivedResults>>(oracleReceivedResults);

    return receipts->tryGettingResult(_result);

}

