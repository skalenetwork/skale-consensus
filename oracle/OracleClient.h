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

#include "OracleReceivedResults.h"
#include "protocols/ProtocolInstance.h"

class Schain;

class OracleRequestBroadcastMessage;

class OracleClient : public ProtocolInstance {
    recursive_mutex m;

    Schain* sChain = nullptr;

    cache::lru_cache< string, ptr< OracleReceivedResults > > receiptsMap;

    string gethURL;

    uint64_t broadcastRequest( ptr< OracleRequestBroadcastMessage > _msg );


public:
    explicit OracleClient( Schain& _sChain );

    uint64_t checkOracleResult( const string& _receipt, string& _result );

    pair< uint64_t, string > submitOracleRequest( const string& _spec, string& _receipt );


    void processResponseMessage( const ptr< MessageEnvelope >& _me );

    void sendTestRequestAndWaitForResult( ptr< OracleRequestSpec > _spec );
};


#endif  // SKALED_ORACLECLIENT_H
