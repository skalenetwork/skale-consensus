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
    auto r = _msg->getHash().toHex();


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

string OracleClient::waitForAnswer(ptr<OracleRequestBroadcastMessage> /*_msg*/) {
    return "{\"result\":\"hihi\"}";
}

void OracleClient::sendTestRequestGet() {
    string _receipt;

    string cid = "\"cid\":" +
                 to_string((uint64_t) getSchain()->getSchainID());
    string uri = "\"uri\":\"http://worldtimeapi.org/api/timezone/Europe/Kiev\"";
    string jsps = "\"jsps\":[\"/unixtime\", \"/day_of_year\", \"/xxx\"]";
    string trims = "\"trims\":[1,1,1]";
    string time = "\"time\":" + to_string(Time::getCurrentTimeMs());
    string pow = "\"pow\":" + string("\"0x0000\"");

    string spec = "{" + cid + "," + uri + "," + jsps + "," + trims + "," + time + "," + pow + "}";
    auto status = runOracleRequest(spec, _receipt);

    CHECK_STATE(status == ORACLE_SUCCESS);

    string result;
}

void OracleClient::sendTestRequestPost() {
    string _receipt;

    string cid = "\"cid\":" +
                 to_string((uint64_t) getSchain()->getSchainID());
    string uri = "\"uri\":\"https://reqres.in/api/users\"";
    string jsps = "\"jsps\":[\"/id\"]";
    string time = "\"time\":" + to_string(Time::getCurrentTimeMs());
    string pow = "\"pow\":" + string("\"0x0000\"");
    string post = "\"post\":\"haha\"";

    string spec = "{" + cid + "," + uri + "," + jsps + "," + time + "," + pow +
            + "," + post + "}";
    auto status = runOracleRequest(spec, _receipt);

    CHECK_STATE(status == ORACLE_SUCCESS);

    string result;
}


uint64_t OracleClient::runOracleRequest(string _spec, string &_receipt) {
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
    auto unsignedResult = msg->getUnsignedOracleResultStr();
    auto sig = msg->getOracleResult()->getSig();

    auto receivedResults = receiptsMap.getIfExists(receipt);

    if (!receivedResults.has_value()) {
        LOG(warn, "Received OracleResponseMessage with unknown receipt" + receipt);
        return;
    }

    auto receipts = std::any_cast<ptr<OracleReceivedResults>>(receivedResults);


    receipts->insertIfDoesntExist(origin, unsignedResult, sig);


    LOG(err, "Processing oracle message:" + to_string(origin));

    string r;

    // tryGettingOracleResult(receipt, r);

}


uint64_t OracleClient::tryGettingOracleResult(string &_receipt,
                                              string &_result) {
    auto oracleReceivedResults = receiptsMap.getIfExists(_receipt);

    if (!oracleReceivedResults.has_value()) {
        LOG(warn, "Received tryGettingOracleResult  with unknown receipt" + _receipt);
        return ORACLE_UNKNOWN_RECEIPT;
    }


    auto receipts = std::any_cast<ptr<OracleReceivedResults>>(oracleReceivedResults);

    return receipts->tryGettingResult(_result);

}

