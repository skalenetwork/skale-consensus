//
// Created by kladko on 04.03.21.
//

#ifndef SKALED_STATUSSERVER_H
#define SKALED_STATUSSERVER_H
#include <jsonrpccpp/server/connectors/httpserver.h>

#include "Log.h"
#include "SkaleCommon.h"

#include "abstractstatusserver.h"
#include "chains/Schain.h"


class StatusServer : public AbstractStatusServer {
    Schain* sChain = nullptr;

public:
    StatusServer( Schain* schain, jsonrpc::AbstractServerConnector& connector,
        jsonrpc::serverVersion_t type );

    virtual string consensus_getTPSAverage();
    virtual string consensus_getBlockSizeAverage();
    virtual string consensus_getBlockTimeAverageMs();
};


#endif  // SKALED_STATUSSERVER_H
