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


class SkaleException;


namespace spdlog {
    class logger;
}

#define __CLASS_NAME__ className( __PRETTY_FUNCTION__ )


#define LOG( __SEVERITY__, __MESSAGE__ )                                                  \
    {                                                                                     \
        std::stringstream __TMP__LOG__STREAM__;                                           \
        __TMP__LOG__STREAM__ << __MESSAGE__;                                              \
        ConsensusEngine::log(                                                             \
            __SEVERITY__, __TMP__LOG__STREAM__.str(), className( __PRETTY_FUNCTION__ ) ); \
    }

#ifdef BITE
#define CATCH_AND_LOG_ANY_EXCEPTION(__LEVEL__, __MESSAGE__)                                      \
    catch (const std::exception& e) {                                                            \
        const std::string __log_msg = std::string(__MESSAGE__) +                                 \
            ": in " + std::string(__FUNCTION__) + ": " + e.what();                               \
        ConsensusEngine::log(__LEVEL__, __log_msg, __CLASS_NAME__);                              \
    } catch (...) {                                                                              \
        const std::string __log_msg = std::string(__MESSAGE__) +                                 \
            ": in " + std::string(__FUNCTION__) + ": Unknown exception";                         \
        ConsensusEngine::log(__LEVEL__, __log_msg, __CLASS_NAME__);                              \
    }


#define CATCH_LOG_AND_RETHROW_ANY_EXCEPTION(__LEVEL__, __MESSAGE__)                          \
    catch (const std::exception& e) {                                                        \
        const std::string __catch_msg = std::string(__MESSAGE__) +                           \
            ": in " + std::string(__FUNCTION__) + ": " + e.what();                           \
        ConsensusEngine::log(__LEVEL__, __catch_msg, __CLASS_NAME__);                         \
        throw_with_nested(InvalidStateException(__catch_msg, __CLASS_NAME__));                \
    } catch (...) {                                                                          \
        const std::string __catch_msg = std::string(__MESSAGE__) +                           \
            ": in " + std::string(__FUNCTION__) + ": Unknown exception";                     \
        ConsensusEngine::log(__LEVEL__, __catch_msg, __CLASS_NAME__);                         \
        throw_with_nested(InvalidStateException(__catch_msg, __CLASS_NAME__));                \
    }
#endif


class SkaleLog {
    ConsensusEngine* engine;

    string prefix = "";

    node_id nodeID;

    shared_ptr< spdlog::logger > mainLogger, proposalLogger, consensusLogger, catchupLogger,
        netLogger, dataStructuresLogger, pendingQueueLogger;

public:
    ConsensusEngine* getEngine() const;

    SkaleLog( node_id _nodeID, ConsensusEngine* _engine );

    const node_id getNodeID() const;

    map< string, shared_ptr< spdlog::logger > > loggers;

    level_enum globalLogLevel;


    void setGlobalLogLevel( string& _s );


    shared_ptr< spdlog::logger > loggerForClass( const char* _className );


    static level_enum logLevelFromString( string& _s );
};

class ExecTimeMeasurement {
private:
    std::string name;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
public:
    ExecTimeMeasurement(const std::string& _name ) {
        name = _name;
        startTime = std::chrono::high_resolution_clock::now();
    }
    ~ExecTimeMeasurement() {
        auto finishTime = std::chrono::high_resolution_clock::now();
        LOG(info, name + std::string(" took ") + std::to_string( std::chrono::duration_cast<std::chrono::milliseconds>(finishTime - startTime).count() ));
    }
};
#endif
