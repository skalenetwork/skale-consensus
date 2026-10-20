#include "SkaleCommon.h"
#include "tests/e2e/harness/ControlChannel.h"
#include "tests/e2e/harness/ConsensusStartup.h"
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

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 45 );
        uint64_t height = 0;
        while ( std::chrono::steady_clock::now() < deadline ) {
            height = static_cast< uint64_t >( engine->getLargestCommittedBlockIDInDb() );
            if ( height > 0 )
                break;
            std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
        }

        if ( height == 0 )
            throw std::runtime_error( "Child did not commit a block before readiness deadline" );

        control.sendResponse( ControlEvent::Ready, { { "height", height } },
            std::chrono::steady_clock::now() + std::chrono::seconds( 10 ) );

        while ( true ) {
            switch ( control.receiveCommand(
                std::chrono::steady_clock::now() + std::chrono::seconds( 120 ) ) ) {
            case ControlCommand::Progress:
                control.sendResponse( ControlEvent::Progress,
                    { { "height",
                        static_cast< uint64_t >( engine->getLargestCommittedBlockIDInDb() ) } },
                    std::chrono::steady_clock::now() + std::chrono::seconds( 10 ) );
                break;
            case ControlCommand::Stop:
                acceptErrorHook.reset();
                engine->testExitGracefullyBlocking();
                engine.reset();
                control.sendResponse( ControlEvent::Stopped, nlohmann::json::object(),
                    std::chrono::steady_clock::now() + std::chrono::seconds( 10 ) );
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
                    std::chrono::steady_clock::now() + std::chrono::seconds( 10 ) );
                break;
            case ControlCommand::ClearAcceptError:
                if ( !acceptErrorHook )
                    throw std::logic_error( "Accept-error injection is not active" );
                acceptErrorHook.reset();
                control.sendResponse( ControlEvent::AcceptErrorCleared, nlohmann::json::object(),
                    std::chrono::steady_clock::now() + std::chrono::seconds( 10 ) );
                break;
            case ControlCommand::AcceptErrorCount:
                control.sendResponse( ControlEvent::AcceptErrorCount,
                    { { "count", acceptErrors->load( std::memory_order_relaxed ) } },
                    std::chrono::steady_clock::now() + std::chrono::seconds( 10 ) );
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
