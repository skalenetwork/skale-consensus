#include "SkaleCommon.h"
#include "tests/harness/transport/ControlChannel.h"
#include "tests/harness/consensus/ConsensusStartup.h"
#include "tests/harness/HarnessTimeouts.h"
#include "network/TCPServerSocket.h"
#include "tests/TestHooks.h"

#include <atomic>
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <thread>
#include <unistd.h>

namespace {
using E2ETestUtils::ControlCommand;
using E2ETestUtils::ControlEvent;
namespace Timeouts = E2ETestUtils::HarnessTimeouts;

}  // namespace

int main( int argc, char** argv ) {
    try {
        E2ETestUtils::ControlChannel control{ Socket{ STDIN_FILENO } };

        if ( argc != 3 )
            throw std::invalid_argument( "Expected configuration and data directories" );

        if ( ::setenv( "DATA_DIR", argv[2], 1 ) != 0 )
            throw std::runtime_error( "Could not set child DATA_DIR" );

        const auto logDirectory =
            ( std::filesystem::path( argv[2] ).parent_path() / "logs" ).string();

        if ( ::setenv( "LOG_DIR", logDirectory.c_str(), 1 ) != 0 )
            throw std::runtime_error( "Could not set child LOG_DIR" );

        E2ETestUtils::ConsensusStartupOptions options;
        options.configDirectory = fs_path( argv[1] );
        auto engine = E2ETestUtils::startConsensusEngine( options );
        auto acceptErrors = std::make_shared< std::atomic< uint64_t > >( 0 );
        std::optional< TestHooks::TCPAcceptErrorHook::Scope > acceptErrorHook;

        const auto deadline = E2ETestUtils::deadlineIn( Timeouts::CHILD_FIRST_BLOCK );
        uint64_t height = 0;
        const auto result = E2ETestUtils::waitUntil(
            [&] {
                height = static_cast< uint64_t >( engine->getLargestCommittedBlockIDInDb() );
                return height > 0;
            },
            deadline, Timeouts::CHILD_READINESS_POLL );
        if ( result == E2ETestUtils::WaitResult::TimedOut )
            throw std::runtime_error( "Child did not commit a block before readiness deadline" );

        control.sendResponse( ControlEvent::Ready, { { "height", height } },
            E2ETestUtils::deadlineIn( Timeouts::CHILD_RESPONSE ) );

        while ( true ) {
            switch ( control.receiveCommand(
                E2ETestUtils::deadlineIn( Timeouts::CHILD_COMMAND_IDLE ) ) ) {
            case ControlCommand::Progress:
                control.sendResponse( ControlEvent::Progress,
                    { { "height",
                        static_cast< uint64_t >( engine->getLargestCommittedBlockIDInDb() ) } },
                    E2ETestUtils::deadlineIn( Timeouts::CHILD_RESPONSE ) );
                break;
            case ControlCommand::Stop:
                acceptErrorHook.reset();
                engine->testExitGracefullyBlocking();
                engine.reset();
                control.sendResponse( ControlEvent::Stopped, nlohmann::json::object(),
                    E2ETestUtils::deadlineIn( Timeouts::CHILD_RESPONSE ) );
                return 0;
            case ControlCommand::InjectAcceptError:
                if ( acceptErrorHook )
                    throw std::logic_error( "Accept-error injection is already active" );
                acceptErrors->store( 0, std::memory_order_relaxed );
                acceptErrorHook.emplace(
                    [acceptErrors]( const TCPServerSocket& listener, int& error ) {
                        if ( listener.getPortType() == port_type::CATCHUP ) {
                            error = EMFILE;
                            acceptErrors->fetch_add( 1, std::memory_order_relaxed );
                        }
                    } );
                control.sendResponse( ControlEvent::AcceptErrorEnabled, nlohmann::json::object(),
                    E2ETestUtils::deadlineIn( Timeouts::CHILD_RESPONSE ) );
                break;
            case ControlCommand::ClearAcceptError:
                if ( !acceptErrorHook )
                    throw std::logic_error( "Accept-error injection is not active" );
                acceptErrorHook.reset();
                control.sendResponse( ControlEvent::AcceptErrorCleared, nlohmann::json::object(),
                    E2ETestUtils::deadlineIn( Timeouts::CHILD_RESPONSE ) );
                break;
            case ControlCommand::AcceptErrorCount:
                control.sendResponse( ControlEvent::AcceptErrorCount,
                    { { "count", acceptErrors->load( std::memory_order_relaxed ) } },
                    E2ETestUtils::deadlineIn( Timeouts::CHILD_RESPONSE ) );
                break;
            }
        }
    } catch ( const std::exception& error ) {
        std::fprintf( stderr, "Consensus test child failed: %s\n", error.what() );
        return 1;
    } catch ( ... ) {
        std::fprintf( stderr, "Consensus test child failed with an unknown exception\n" );
        return 1;
    }
}
