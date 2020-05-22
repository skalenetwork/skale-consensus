/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file Log.h
    @author Stan Kladko
    @date 2018
*/

#ifndef _LOG_H
#define _LOG_H


#include <stdlib.h>
#include <iostream>
#include <map>
#include <memory>

#include "spdlog/spdlog.h"

#include "exceptions/FatalError.h"
#include "exceptions/InvalidArgumentException.h"
#include "exceptions/InvalidStateException.h"

#include "SkaleCommon.h"
#include "node/ConsensusEngine.h"

using namespace std;



class Exception;


namespace spdlog {
class logger;
}

#define __CLASS_NAME__ className( __PRETTY_FUNCTION__ )

#define LOG( __SEVERITY__, __MESSAGE__ ) \
    ConsensusEngine::log( __SEVERITY__, __MESSAGE__, className( __PRETTY_FUNCTION__ ) )


class SkaleLog {


    ConsensusEngine* engine;

    shared_ptr< string > prefix = nullptr;

    node_id nodeID;

    shared_ptr< spdlog::logger > mainLogger, proposalLogger, consensusLogger, catchupLogger,
        netLogger, dataStructuresLogger, pendingQueueLogger;

public:

    ConsensusEngine *getEngine() const;

    SkaleLog( node_id _nodeID, ConsensusEngine* _engine);

    const node_id getNodeID() const;

    map< string, shared_ptr< spdlog::logger > > loggers;

    level_enum globalLogLevel;


    void setGlobalLogLevel( string& _s );


    shared_ptr< spdlog::logger > loggerForClass( const char* _className );


    static level_enum logLevelFromString(string &_s);
};
#endif
