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
#include "network/Network.h"
#include "utils/Time.h"
#include "protocols/ProtocolInstance.h"
#include "OracleRequestBroadcastMessage.h"
#include "OracleClient.h"


OracleClient::OracleClient(Schain& _sChain) : ProtocolInstance(ORACLE, _sChain), sChain(&_sChain) {
}

string OracleClient::broadcastRequestAndWaitForAnswer(ptr<OracleRequestBroadcastMessage> _msg) {
    CHECK_STATE(_msg)
    CHECK_STATE(sChain)
    sChain->getNode()->getNetwork()->broadcastOracleMessage(_msg);

    auto result = waitForAnswer(_msg);
    return result;

}

string OracleClient::waitForAnswer(ptr<OracleRequestBroadcastMessage> /*_msg*/ ) {
    usleep(100000);
    return "";
}

void OracleClient::sendTestRequest() {
    string result = broadcastRequestAndWaitForAnswer(nullptr);
    LOG(info, "Oracle result:\n" + result);
}


string OracleClient::runOracleRequestResponse(string _spec) {
    auto msg = new OracleRequestBroadcastMessage(_spec,  sChain->getLastCommittedBlockID(),
                                                 Time::getCurrentTimeMs(),
                                                 *sChain->getOracleClient());
}
