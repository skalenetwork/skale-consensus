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

    @file Log.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"

#include "exceptions/ParsingException.h"

#include "Log.h"
#include "exceptions/ConnectionRefusedException.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"


using namespace std;





void SkaleLog::setGlobalLogLevel(string &_s) {
    globalLogLevel = logLevelFromString(_s);

    for (auto &&item : loggers) {
        item.second->set_level(globalLogLevel);
    }

    ConsensusEngine::setConfigLogLevel(_s);
}



string level_names[]   = SPDLOG_LEVEL_NAMES;
level_enum SkaleLog::logLevelFromString(string &_s) {
    for (int i = 0; i < 7; i++) {
        if (_s == level_names[i]) {
            return level_enum(i);
        }
    }


    BOOST_THROW_EXCEPTION(ParsingException("Unknown level name " + _s, __CLASS_NAME__));
}

shared_ptr<spdlog::logger> SkaleLog::loggerForClass(const char *_s) {
    string key;

    if (strstr(_s, "Proposal"))
        key = "Proposal";
    if (strstr(_s, "Catchup"))
        key = "Catchup";

    if (strstr(_s, "Pending"))
        key = "Pending";
    if (strstr(_s, "Consensus"))
        key = "Consensus";
    if (strstr(_s, "Protocol"))
        key = "Consensus";
    if (strstr(_s, "Header"))
        key = "Datastructures";
    if (strstr(_s, "Network"))
        key = "Net";

    if (key == "")
        key = "Main";

    CHECK_STATE(loggers.count(key) > 0);
    return loggers[key];
}

SkaleLog::SkaleLog(node_id _nodeID, ConsensusEngine* _engine) {

    CHECK_STATE(_engine);

    engine = _engine;

    nodeID = _nodeID;

    prefix = to_string(_nodeID) + ":";


    if (_engine->getEngineID() > 1) {
        prefix = to_string(_engine->getEngineID()) + ":" + to_string(_nodeID) + ":";
    } else {
        prefix = to_string(_nodeID) + ":";
    }

    mainLogger = _engine->createLogger(prefix + "main");
    loggers["Main"] = mainLogger;
    proposalLogger = _engine->createLogger(prefix + "proposal");
    loggers["Proposal"] = proposalLogger;
    catchupLogger = _engine->createLogger(prefix + "catchup");
    loggers["Catchup"] = catchupLogger;
    consensusLogger = _engine->createLogger(prefix + "consensus");
    loggers["Consensus"] = consensusLogger;
    netLogger = _engine->createLogger(prefix + "net");
    loggers["Net"] = netLogger;
    dataStructuresLogger = _engine->createLogger(prefix + "datastructures");
    loggers["Datastructures"] = dataStructuresLogger;
    pendingQueueLogger = _engine->createLogger(prefix + "pending");
    loggers["Pending"] = pendingQueueLogger;
}



const node_id SkaleLog::getNodeID() const {
    return nodeID;
}

ConsensusEngine * SkaleLog::getEngine() const {
    CHECK_STATE(engine);
    return engine;
}



