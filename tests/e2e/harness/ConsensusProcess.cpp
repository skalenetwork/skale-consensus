#include "ConsensusProcess.h"
#include "TestSockets.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <signal.h>
#include <spawn.h>
#include <stdexcept>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <utility>

extern char** environ;

namespace E2ETestUtils {
namespace {

class SpawnActions {
    posix_spawn_file_actions_t actions;

public:
    SpawnActions() {
        const int error = ::posix_spawn_file_actions_init( &actions );
        if ( error != 0 )
            throw std::system_error( error, std::generic_category(), "Could not init spawn actions" );
    }
    ~SpawnActions() { ::posix_spawn_file_actions_destroy( &actions ); }
    SpawnActions( const SpawnActions& ) = delete;
    SpawnActions& operator=( const SpawnActions& ) = delete;

    void redirectControl( int fd ) {
        const int error = ::posix_spawn_file_actions_adddup2( &actions, fd, STDIN_FILENO );
        if ( error != 0 )
            throw std::system_error( error, std::generic_category(), "Could not redirect control" );
    }
    const posix_spawn_file_actions_t* get() const { return &actions; }
};

template < typename Update >
void writeConfig( const std::filesystem::path& source,
    const std::filesystem::path& destination, Update update ) {
    std::ifstream input( source );
    if ( !input )
        throw std::runtime_error( "Could not open test config: " + source.string() );
    nlohmann::json json;
    input >> json;
    update( json );
    std::ofstream output( destination );
    output << json.dump( 2 );
    output.close();
    if ( !output )
        throw std::runtime_error( "Could not write test config: " + destination.string() );
}

} // namespace

ConsensusProcess::ConsensusProcess( const std::string& executable, const std::string& sourceRoot,
    const std::vector< std::string >& extraArguments ) {
    try {
        prepareEnvironment( sourceRoot );
        launchChild( executable, extraArguments );
        waitUntilReady( deadlineIn( HarnessTimeouts::PARENT_READINESS ) );
    } catch ( ... ) {
        rethrowChildFailure( "Consensus child startup failed" );
    }
}

ConsensusProcess::~ConsensusProcess() {
    cleanup( !stoppedSuccessfully || std::uncaught_exceptions() > 0 );
}

void ConsensusProcess::prepareEnvironment( const std::string& sourceRoot ) {
    char temporary[] = "/tmp/consensus_process_XXXXXX";
    const auto created = ::mkdtemp( temporary );
    if ( created == nullptr )
        throw std::system_error( errno, std::generic_category(), "mkdtemp" );
    directory = created;

    // Separate loopback addresses isolate this process's listener port range.
    const auto id = static_cast< unsigned >( ::getpid() );
    bindIP = "127." + std::to_string( 1 + ( id >> 16 ) % 254 ) + "." +
             std::to_string( ( id >> 8 ) & 255 ) + "." + std::to_string( id & 255 );
    const auto config = directory / "config/node1";
    std::filesystem::create_directories( config / "schains" );
    std::filesystem::create_directories( directory / "data" );
    std::filesystem::create_directories( directory / "logs" );
    writeConfig( std::filesystem::path( sourceRoot ) / "test/onenode/node1/Node.json",
        config / "Node.json", [this]( auto& json ) {
            json["bindIP"] = bindIP;
            json["basePort"] = BASE_PORT;
        } );
    writeConfig( std::filesystem::path( sourceRoot ) / "test/onenode/node1/schains/schain1.json",
        config / "schains/schain1.json", [this]( auto& json ) {
            json["nodes"][0]["ip"] = bindIP;
            json["nodes"][0]["basePort"] = BASE_PORT;
        } );
}

void ConsensusProcess::launchChild(
    const std::string& executable, const std::vector< std::string >& extraArguments ) {
    std::vector< std::string > arguments{
        executable, ( directory / "config" ).string(), ( directory / "data" ).string() };
    arguments.insert( arguments.end(), extraArguments.begin(), extraArguments.end() );
    std::vector< char* > args;
    args.reserve( arguments.size() + 1 );
    for ( auto& argument : arguments )
        args.push_back( argument.data() );
    args.push_back( nullptr );
    auto sockets = createTestSocketPair();
    control = ControlChannel{ std::move( sockets[0] ) };
    SpawnActions actions;
    actions.redirectControl( sockets[1].get() );
    const int error = ::posix_spawn(
        &pid, executable.c_str(), actions.get(), nullptr, args.data(), environ );
    if ( error != 0 ) {
        pid = -1;
        throw std::system_error( error, std::generic_category(), "Could not spawn consensus child" );
    }
    const int closeError = sockets[1].reset();
    if ( closeError != 0 )
        std::fprintf( stderr, "Could not close test socket: %s\n", std::strerror( closeError ) );
}

uint64_t ConsensusProcess::receiveHeight( ControlEvent event, Deadline deadline ) {
    try {
        const auto response = control.receiveResponse( event, deadline );
        const auto& height = response.at( "height" );
        if ( !height.is_number_unsigned() )
            throw std::runtime_error( "Invalid consensus child block height" );
        return height.get< uint64_t >();
    } catch ( ... ) {
        control.close();
        throw;
    }
}

void ConsensusProcess::waitUntilReady( Deadline deadline ) {
    if ( receiveHeight( ControlEvent::Ready, deadline ) == 0 )
        throw std::runtime_error( "Consensus child did not report committed-block readiness" );
}

uint64_t ConsensusProcess::progress( Deadline deadline ) {
    try {
        control.sendCommand( ControlCommand::Progress, deadline );
        return receiveHeight( ControlEvent::Progress, deadline );
    } catch ( ... ) {
        rethrowChildFailure( "Consensus child progress failed" );
    }
}

void ConsensusProcess::injectAcceptError( Deadline deadline ) {
    try {
        control.sendCommand( ControlCommand::InjectAcceptError, deadline );
        control.receiveResponse( ControlEvent::AcceptErrorEnabled, deadline );
    } catch ( ... ) {
        rethrowChildFailure( "Enabling child accept-error injection failed" );
    }
}

void ConsensusProcess::clearAcceptError( Deadline deadline ) {
    try {
        control.sendCommand( ControlCommand::ClearAcceptError, deadline );
        control.receiveResponse( ControlEvent::AcceptErrorCleared, deadline );
    } catch ( ... ) {
        rethrowChildFailure( "Clearing child accept-error injection failed" );
    }
}

uint64_t ConsensusProcess::acceptErrorCount( Deadline deadline ) {
    try {
        control.sendCommand( ControlCommand::AcceptErrorCount, deadline );
        const auto response = control.receiveResponse( ControlEvent::AcceptErrorCount, deadline );
        const auto& count = response.at( "count" );
        if ( !count.is_number_unsigned() )
            throw std::runtime_error( "Invalid child accept-error count" );
        return count.get< uint64_t >();
    } catch ( ... ) {
        rethrowChildFailure( "Reading child accept-error count failed" );
    }
}

void ConsensusProcess::stop( Deadline deadline ) {
    try {
        control.sendCommand( ControlCommand::Stop, deadline );
        control.receiveResponse( ControlEvent::Stopped, deadline );
        const auto result = waitUntil(
            [this] {
                collectChildStatus( false );
                if ( childStatus ) {
                    if ( !WIFEXITED( *childStatus ) || WEXITSTATUS( *childStatus ) != 0 )
                        throw std::runtime_error( "Consensus child exited unsuccessfully" );
                    return true;
                }
                if ( waitError != 0 )
                    throw std::system_error(
                        waitError, std::generic_category(), "Could not wait for consensus child" );
                return false;
            },
            deadline, HarnessTimeouts::CHILD_REAP_POLL );
        if ( result == WaitResult::TimedOut )
            throw std::runtime_error( "Consensus child did not exit after shutdown" );
        stoppedSuccessfully = true;
    } catch ( ... ) {
        rethrowChildFailure( "Consensus child shutdown failed" );
    }
}

void ConsensusProcess::collectChildStatus( bool blocking ) noexcept {
    if ( pid <= 0 )
        return;
    int status = 0;
    pid_t result;
    do {
        result = ::waitpid( pid, &status, blocking ? 0 : WNOHANG );
    } while ( result < 0 && errno == EINTR );
    if ( result == pid ) {
        childStatus = status;
        waitError = 0;
        pid = -1;
    } else if ( result < 0 ) {
        waitError = errno;
        std::fprintf( stderr, "Could not reap consensus child: %s\n", std::strerror( waitError ) );
        // Never signal a PID whose child ownership can no longer be established.
        if ( waitError == ECHILD )
            pid = -1;
    }
}

void ConsensusProcess::terminateChild() noexcept {
    collectChildStatus( false );
    if ( pid <= 0 )
        return;
    if ( ::kill( pid, SIGKILL ) == 0 ) {
        killRequested = true;
    } else if ( errno != ESRCH ) {
        std::fprintf( stderr, "Could not kill consensus child: %s\n", std::strerror( errno ) );
        return;
    }
    collectChildStatus( true );
}

std::string ConsensusProcess::describeChildStatus() const {
    if ( childStatus ) {
        if ( WIFEXITED( *childStatus ) )
            return "child exited with code " + std::to_string( WEXITSTATUS( *childStatus ) );
        if ( WIFSIGNALED( *childStatus ) ) {
            const int signal = WTERMSIG( *childStatus );
            std::string result = "child terminated by signal " + std::to_string( signal );
            if ( signal == SIGKILL && killRequested )
                result += " (supervisor-requested SIGKILL)";
            return result;
        }
        return "child returned unexpected wait status " + std::to_string( *childStatus );
    }
    if ( waitError != 0 )
        return std::string( "child exit status unavailable: " ) + std::strerror( waitError );
    return pid > 0 ? "child is still running" : "child was not started";
}

void ConsensusProcess::rethrowChildFailure( const char* operation ) {
    terminateChild();
    control.close();
    const std::string context = std::string( operation ) + "; " + describeChildStatus() +
        "; diagnostics: " + ( directory.empty() ? "not created" : directory.string() );
    try {
        throw;
    } catch ( const std::exception& error ) {
        std::throw_with_nested( std::runtime_error( context + "; cause: " + error.what() ) );
    } catch ( ... ) {
        std::throw_with_nested( std::runtime_error( context + "; cause: unknown exception" ) );
    }
}

void ConsensusProcess::cleanup( bool preserveDiagnostics ) noexcept {
    terminateChild();
    control.close();
    if ( !directory.empty() ) {
        if ( preserveDiagnostics ) {
            if ( childStatus ) {
                if ( WIFEXITED( *childStatus ) )
                    std::fprintf( stderr, "Consensus child exited with code %d\n",
                        WEXITSTATUS( *childStatus ) );
                else if ( WIFSIGNALED( *childStatus ) )
                    std::fprintf( stderr, "Consensus child terminated by signal %d%s\n",
                        WTERMSIG( *childStatus ),
                        WTERMSIG( *childStatus ) == SIGKILL && killRequested ?
                            " (supervisor-requested SIGKILL)" : "" );
            }
            std::fprintf( stderr, "Consensus test diagnostics preserved in %s (logs in logs/)\n",
                directory.c_str() );
            return;
        }
        std::error_code error;
        std::filesystem::remove_all( directory, error );
        if ( error )
            std::fprintf( stderr, "Could not remove test directory %s: %s\n",
                directory.c_str(), error.message().c_str() );
    }
}

} // namespace E2ETestUtils
