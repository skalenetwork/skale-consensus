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
#include "OracleRequestBroadcastMessage.h"
#include "chains/Schain.h"
#include "messages/MessageEnvelope.h"
#include "network/Network.h"
#include "OracleResponseMessage.h"
#include "utils/Time.h"
#include "protocols/ProtocolInstance.h"
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


    auto exists = receiptsMap.putIfDoesNotExist(r, make_shared<map<uint64_t, string>>());

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

    if (!this->receiptsMap.exists(receipt)) {
        LOG(warn, "Received OracleResponseMessage with unknown receipt" + receipt);
        return;
    }

    auto result = receiptsMap.getIfExists(receipt);

    if (!result.has_value()) {
        LOG(warn, "Received OracleResponseMessage with unknown receipt" + receipt);
        return;
    }

    auto receipts = std::any_cast<ptr<map<uint64_t, string>>>(result);

    LOCK(m)

    if (receipts->count(origin) > 0) {
        LOG(warn, "Duplicate OracleResponseMessage for receipt:" + receipt +
             " index:" + to_string(origin));
        return;
    }

    if (receipts->size() > getSchain()->getRequiredSigners()) {
        return;
    }

    receipts->insert({origin, msg->getOracleResult()});

    if (receipts->size() == getSchain()->getRequiredSigners()) {
        LOG(err, "Processing oracle messages:" + to_string(origin));
    }
}