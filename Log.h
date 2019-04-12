/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file Log.h
    @author Stan Kladko
    @date 2018
*/

#ifndef _LOG_H
#define _LOG_H


#include <stdlib.h>
#include <iostream>
#include <memory>

#include "spdlog/spdlog.h"

using namespace std;

using namespace spdlog::level;



#define __CLASS_NAME__ className( __PRETTY_FUNCTION__ )

#define LOG(__SEVERITY__, __MESSAGE__) logThreadLocal_->log(__SEVERITY__, __MESSAGE__)

class Exception;


namespace spdlog {
    class logger;
}

class Log {



    recursive_mutex logLock;


    static shared_ptr<string>dataDir;

    static shared_ptr<string> logFileNamePrefix;

    static shared_ptr<spdlog::logger> configLogger;

    static shared_ptr<spdlog::sinks::sink> rotatingFileSync;

    shared_ptr<string> prefix = nullptr;


    node_id nodeID;

private:


    shared_ptr<spdlog::logger> mainLogger, proposalLogger, consensusLogger, catchupLogger, netLogger,
            dataStructuresLogger, pendingQueueLogger;

public:


    Log(node_id _nodeID);

    const node_id &getNodeID() const;

    map<string, shared_ptr<spdlog::logger>> loggers;

    level_enum globalLogLevel;

    static void init();

    static void setConfigLogLevel(string& _s);


    void setGlobalLogLevel(string& _s);

    static level_enum logLevelFromString(string& _s);


    shared_ptr<spdlog::logger> loggerForClass(const char *_className);

    static void log(level_enum _severity, const string &_message);


    static shared_ptr<spdlog::logger> createLogger(const string &loggerName);

    static const shared_ptr<string> &getDataDir();
};

#endif
