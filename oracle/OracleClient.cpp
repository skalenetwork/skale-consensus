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
#include "node/Node.h"
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

uint64_t OracleClient::broadcastRequest(ptr<OracleRequestBroadcastMessage> _msg) {

    try {

        CHECK_STATE(_msg)
        CHECK_STATE(sChain)


        auto receipt = _msg->getParsedSpec()->getReceipt();


        auto results = make_shared<OracleReceivedResults>(
                _msg->getParsedSpec(), getSchain()->getRequiredSigners(),
                (uint64_t) getSchain()->getNodeCount(), getSchain()->getNode()->isSgxEnabled());

        auto exists = receiptsMap.putIfDoesNotExist(receipt,
                                                    results);

        if (!exists) {
            LOG(err, "Request exists:" + receipt);
            return ORACLE_DUPLICATE_REQUEST;
        }

        LOCK(m);

        sChain->getNode()->getNetwork()->broadcastOracleRequestMessage(_msg);

        return ORACLE_SUCCESS;

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void OracleClient::sendTestRequestGet() {


    string uri = "http://worldtimeapi.org/api/timezone/Europe/Kiev";
    vector<string> jsps{"/unixtime", "/day_of_year", "/xxx"};
    vector<uint64_t> trims{1, 1, 1};
    string post = "";
    string encoding;

    //encoding = ORACLE_ENCODING_ABI;
    //sendTestWebRequestAndWaitForResult(uri, jsps, trims, post, encoding);
    encoding = ORACLE_ENCODING_JSON;
    sendTestWebRequestAndWaitForResult(uri, jsps, trims, post, encoding);
}


void OracleClient::sendTestRequestPost() {

    try {

        if (getSchain()->getSchainIndex() != 1) {
            return;
        }

        string _receipt;
        string uri = "https://reqres.in/api/users";
        vector<string> jsps = {"/id"};
        string post = "haha";
        string encoding = "rlp";

        sendTestWebRequestAndWaitForResult(uri, jsps, {}, post, encoding);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void OracleClient::sendTestWebRequestAndWaitForResult(const string &_uri,
                                                      const vector<string> &_jsps,
                                                      const vector<uint64_t> &_trims,
                                                      const string &_post,
                                                      const string &_encoding) {


    try {

        auto _cid = (uint64_t) getSchain()->getSchainID();

        string _receipt;


        auto time = Time::getCurrentTimeMs();

        auto os = OracleRequestSpec::makeWebSpec(_cid, _uri, _jsps, _trims, _post, _encoding, time);


        auto status = submitOracleRequest(os->getSpec(), _receipt);

        CHECK_STATE(status == ORACLE_SUCCESS);


        thread t([this, _receipt]() {
            while (true) {
                string result;
                string r = _receipt;
                sleep(1);
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

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
};


void OracleClient::sendTestEthCallRequestAndWaitForResult(
    const string &_uri,
    const string& _from,
    const string& _to,
    const string& _data,
    const string& _gas,
    const string& _block,
    const string &_encoding) {


    try {

        auto _cid = (uint64_t) getSchain()->getSchainID();

        string _receipt;


        auto time = Time::getCurrentTimeMs();

        auto os = OracleRequestSpec::makeEthCallSpec(_cid, _uri, _from, _to,
            _data, _gas, _block, _encoding, time);


        auto status = submitOracleRequest(os->getSpec(), _receipt);

        CHECK_STATE(status == ORACLE_SUCCESS);


        thread t([this, _receipt]() {
            while (true) {
                string result;
                string r = _receipt;
                sleep(1);
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

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
};




uint64_t OracleClient::submitOracleRequest(const string &_spec, string &_receipt) {

    try {

        auto msg = make_shared<OracleRequestBroadcastMessage>(_spec, sChain->getLastCommittedBlockID(),
                                                              Time::getCurrentTimeMs(),
                                                              *sChain->getOracleClient());
        _receipt = msg->getParsedSpec()->getReceipt();

        return broadcastRequest(msg);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void OracleClient::processResponseMessage(const ptr<MessageEnvelope> &_me) {
    try {
        CHECK_STATE(_me);

        auto origin = (uint64_t) _me->getSrcSchainIndex();

        CHECK_STATE(origin > 0 || origin <= getSchain()->getNodeCount());

        auto msg = dynamic_pointer_cast<OracleResponseMessage>(_me->getMessage());

        CHECK_STATE(msg);

        auto receipt = msg->getReceipt();

        auto receivedResults = receiptsMap.getIfExists(receipt);

        if (!receivedResults.has_value()) {
            LOG(warn, "Received OracleResponseMessage with unknown receipt" + receipt);
            return;
        }

        auto rslts = std::any_cast<ptr<OracleReceivedResults>>(receivedResults);
        rslts->insertIfDoesntExist(origin, msg->getOracleResult(rslts->getRequestSpec(),
                                                                getSchain()->getSchainID()));
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


uint64_t OracleClient::checkOracleResult(const string &_receipt,
                                         string &_result) {

    try {
        auto oracleReceivedResults = receiptsMap.getIfExists(_receipt);

        if (!oracleReceivedResults.has_value()) {
            LOG(warn, "Received tryGettingOracleResult  with unknown receipt" + _receipt);
            return ORACLE_UNKNOWN_RECEIPT;
        }


        auto receipts = std::any_cast<ptr<OracleReceivedResults>>(oracleReceivedResults);

        return receipts->tryGettingResult(_result);

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

