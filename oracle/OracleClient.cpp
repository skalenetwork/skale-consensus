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
#include "OracleReceivedResults.h"
#include "OracleErrors.h"
#include "OracleRequestBroadcastMessage.h"
#include "OracleClient.h"


OracleClient::OracleClient(Schain& _sChain) : ProtocolInstance(ORACLE, _sChain), sChain(&_sChain),
                                              receiptsMap(ORACLE_RECEIPTS_MAP_SIZE){
}

uint64_t OracleClient::broadcastRequestAndReturnReceipt(ptr<OracleRequestBroadcastMessage> _msg, string& receipt) {

    CHECK_STATE(_msg)
    CHECK_STATE(sChain)
    auto r = _msg->getHash().toHex();


    auto exists = receiptsMap.putIfDoesNotExist(r, make_shared<OracleReceivedResults>());

    if (!exists) {
        LOG(err, "Request exists:" + r);
        return ORACLE_DUPLICATE_REQUEST;
    }

    LOCK(m);
    sChain->getNode()->getNetwork()->broadcastOracleRequestMessage(_msg);
    receipt = r;
    return ORACLE_SUCCESS;
}

string OracleClient::waitForAnswer(ptr<OracleRequestBroadcastMessage> /*_msg*/ ) {
    return "{\"result\":\"hihi\"}";
}

void OracleClient::sendTestRequest() {
    string result;

    auto status = runOracleRequest("{\"request\":\"haha\"}", result);
    CHECK_STATE(status == ORACLE_SUCCESS);
}


uint64_t OracleClient::runOracleRequest(string _spec, string result) {
    auto msg = make_shared<OracleRequestBroadcastMessage>(_spec,  sChain->getLastCommittedBlockID(),
                                                 Time::getCurrentTimeMs(),
                                                 *sChain->getOracleClient());
    return broadcastRequestAndReturnReceipt(msg, result);
}


void OracleClient::processResponseMessage(const ptr<MessageEnvelope> &_me) {
    CHECK_STATE(_me);

    auto msg = dynamic_pointer_cast<OracleResponseMessage>(_me->getMessage());


    CHECK_STATE(msg);

    auto origin = (uint64_t) _me->getSrcSchainIndex();

    CHECK_STATE(origin > 0 || origin <= getSchain()->getNodeCount());

    auto receipt = msg->getReceipt();
    auto res = msg->getOracleResult();

    auto receivedResults = receiptsMap.getIfExists(receipt);

    if (!receivedResults.has_value()) {
        LOG(warn, "Received OracleResponseMessage with unknown receipt" + receipt);
        return;
    }

    auto receipts = std::any_cast<ptr<OracleReceivedResults>>(receivedResults);

    LOCK(m)

    if (receipts->resultsBySchainIndex->count(origin) > 0) {
        LOG(warn, "Duplicate OracleResponseMessage for receipt:" + receipt +
             " index:" + to_string(origin));
        return;
    }


    receipts->resultsBySchainIndex->insert({origin, res});
    if (receipts->resultsByCount->count(res) == 0) {
        receipts->resultsByCount->insert({res, 1});
    } else {
        auto count = receipts->resultsByCount->at(res);
        receipts->resultsByCount->insert({res, count + 1});
    }

    LOG(err, "Processing oracle message:" + to_string(origin));
}


uint64_t OracleClient::tryGettingOracleResult(string& _receipt ,
                                              string& _result ) {
    auto oracleReceivedResults = receiptsMap.getIfExists(_receipt);

    if (!oracleReceivedResults.has_value()) {
        LOG(warn, "Received tryGettingOracleResult  with unknown receipt" + _receipt);
        return ORACLE_UNKNOWN_RECEIPT;
    }


    auto receipts = std::any_cast<ptr<OracleReceivedResults>>(oracleReceivedResults);

    for (auto&& item : *receipts->resultsByCount) {
        if (item.second >= getSchain()->getRequiredSigners()) {
            _result = item.first;
            return ORACLE_SUCCESS;
        }
    }

    return ORACLE_RESULT_NOT_READY;;

}

