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
