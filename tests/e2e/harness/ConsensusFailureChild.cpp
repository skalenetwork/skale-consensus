#include "ControlChannel.h"
#include "HarnessTimeouts.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <sys/resource.h>
#include <unistd.h>

namespace {
[[noreturn]] void fail( const std::string& mode ) {
    if ( mode.find( "signal" ) != std::string::npos ) {
        std::abort();
    }
    ::_exit( 7 );
}
}

int main( int argc, char** argv ) {
    try {
        if ( argc != 4 )
            throw std::invalid_argument( "Expected configuration and data directories and failure mode" );
        const rlimit noCore{ 0, 0 };
        if ( ::setrlimit( RLIMIT_CORE, &noCore ) != 0 )
            throw std::runtime_error( "Could not disable fixture core dumps" );
        const std::string mode = argv[3];
        if ( mode.find( "startup" ) == 0 )
            fail( mode );

        using namespace E2ETestUtils;
        ControlChannel control{ Socket{ STDIN_FILENO } };
        control.sendResponse( ControlEvent::Ready, { { "height", uint64_t{ 1 } } },
            deadlineIn( HarnessTimeouts::FAILURE_FIXTURE_EXCHANGE ) );
        const auto command = control.receiveCommand(
            deadlineIn( HarnessTimeouts::FAILURE_FIXTURE_EXCHANGE ) );
        if ( mode == "progress-timeout" ) {
            for ( ;; )
                ::pause();
        }
        if ( mode == "stop-ack-exit" ) {
            if ( command != ControlCommand::Stop )
                throw std::runtime_error( "Expected stop command" );
            control.sendResponse( ControlEvent::Stopped, nlohmann::json::object(),
                deadlineIn( HarnessTimeouts::FAILURE_FIXTURE_EXCHANGE ) );
        }
        fail( mode );
    } catch ( const std::exception& error ) {
        std::fprintf( stderr, "Consensus failure fixture failed: %s\n", error.what() );
        return 2;
    }
}
