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


#ifndef SKALED_ORACLECLIENT_H
#define SKALED_ORACLECLIENT_H

#include "protocols/ProtocolInstance.h"
#include "OracleReceivedResults.h"

class Schain;
class OracleRequestBroadcastMessage;

class OracleClient : public ProtocolInstance {

    recursive_mutex m;

    Schain* sChain = nullptr;

    cache::lru_cache<string, ptr<OracleReceivedResults>> receiptsMap;


    string waitForAnswer(ptr<OracleRequestBroadcastMessage> /*_msg*/ );

    uint64_t  tryGettingOracleResult(string& _receipt, string& _result);

public:
    explicit OracleClient(Schain& _sChain);

    uint64_t runOracleRequest(string _spec, string &_receipt);

    uint64_t broadcastRequestAndReturnReceipt(ptr<OracleRequestBroadcastMessage> _msg, string& _receipt);

    void sendTestRequest();


    void processResponseMessage(const ptr<MessageEnvelope> &_me);

};






#endif //SKALED_ORACLECLIENT_H
