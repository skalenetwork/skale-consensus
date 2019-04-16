//
// Created by stan on 19.02.18.
//


#include "SkaleConfig.h"

#include "exceptions/ParsingException.h"

#include "Log.h"
#include "exceptions/ConnectionRefusedException.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"


using namespace std;


void Log::init() {
    spdlog::flush_every( std::chrono::seconds( 1 ) );

    logThreadLocal_ = nullptr;

    char* d = std::getenv( "DATA_DIR" );

    if ( d != nullptr ) {
        dataDir = make_shared< string >( d );
        cerr << "Found data dir:" << *dataDir << endl;
        logFileNamePrefix = make_shared< string >( *dataDir + "/skaled.log" );
        rotatingFileSync = make_shared< spdlog::sinks::rotating_file_sink_mt >(
            *logFileNamePrefix, 10* 1024 * 1024, 5 );
    } else {
        dataDir = make_shared< string >( "/tmp" );
        logFileNamePrefix = nullptr;
        rotatingFileSync = nullptr;
    }

    configLogger = createLogger( "config" );
}

shared_ptr< spdlog::logger > Log::createLogger( const string& loggerName ) {
    shared_ptr< spdlog::logger > logger = spdlog::get( loggerName );

    if ( !logger ) {
        if ( logFileNamePrefix != nullptr ) {
            logger = spdlog::get( loggerName );
            logger = make_shared< spdlog::logger >( loggerName, rotatingFileSync );
            logger->flush_on( info );

        } else {
            logger = spdlog::stdout_color_mt( loggerName );
        }
    }
    return logger;
}

void Log::setGlobalLogLevel( string& _s ) {
    globalLogLevel = logLevelFromString( _s );

    for ( auto&& item : loggers ) {
        item.second->set_level( globalLogLevel );
    }
}

void Log::setConfigLogLevel( string& _s ) {
    auto configLogLevel = logLevelFromString( _s );

    Log::configLogger->set_level( configLogLevel );
}


level_enum Log::logLevelFromString( string& _s ) {
    for ( int i = 0; i < 7; i++ ) {
        if ( _s == level_names[i] ) {
            return level_enum( i );
        }
    }


    throw ParsingException( "Unknown level name " + _s , __CLASS_NAME__);
}

shared_ptr< spdlog::logger > Log::loggerForClass( const char* _s ) {
    string key;

    if ( strstr( _s, "Proposal" ) )
        key = "Proposal";
    if ( strstr( _s, "Catchup" ) )
        key = "Catchup";

    if ( strstr( _s, "Pending" ) )
        key = "Pending";
    if ( strstr( _s, "Consensus" ) )
        key = "Consensus";
    if ( strstr( _s, "Protocol" ) )
        key = "Consensus";
    if ( strstr( _s, "Header" ) )
        key = "Datastructures";
    if ( strstr( _s, "Network" ) )
        key = "Net";

    if ( key == "" )
        key = "Main";

    assert( loggers.count( key ) > 0 );
    return loggers[key];
}

Log::Log( node_id _nodeID ) {
    nodeID = _nodeID;

    prefix = make_shared< string >( to_string( _nodeID ) + ":" );


    mainLogger = createLogger( *prefix + "main" );
    loggers["Main"] = mainLogger;
    proposalLogger = createLogger( *prefix + "proposal" );
    loggers["Proposal"] = proposalLogger;
    catchupLogger = createLogger( *prefix + "catchup" );
    loggers["Catchup"] = catchupLogger;
    consensusLogger = createLogger( *prefix + "consensus" );
    loggers["Consensus"] = consensusLogger;
    netLogger = createLogger( *prefix + "net" );
    loggers["Net"] = netLogger;
    dataStructuresLogger = createLogger( *prefix + "datastructures" );
    loggers["Datastructures"] = dataStructuresLogger;
    pendingQueueLogger = createLogger( *prefix + "pending" );
    loggers["Pending"] = pendingQueueLogger;
}

void Log::log( level_enum _severity, const string& _message ) {
    if ( logThreadLocal_ == nullptr ) {
        configLogger->log( _severity, _message );
    } else {
        logThreadLocal_->loggerForClass( __CLASS_NAME__.c_str() )->log( _severity, _message );
    }
}



const node_id& Log::getNodeID() const {
    return nodeID;
}

const shared_ptr< string >& Log::getDataDir() {
    ASSERT( dataDir );
    return dataDir;
}


ptr< spdlog::logger > Log::configLogger = nullptr;


ptr< spdlog::sinks::sink > Log::rotatingFileSync = nullptr;


ptr< string > Log::logFileNamePrefix = nullptr;


ptr< string > Log::dataDir = nullptr;
