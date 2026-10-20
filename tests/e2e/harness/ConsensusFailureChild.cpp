#include "ControlChannel.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
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
        if ( argc != 3 )
            throw std::invalid_argument( "Expected configuration and data directories" );
        const rlimit noCore{ 0, 0 };
        if ( ::setrlimit( RLIMIT_CORE, &noCore ) != 0 )
            throw std::runtime_error( "Could not disable fixture core dumps" );
        const auto mode = std::filesystem::path( argv[0] ).filename().string();
        if ( mode.find( "startup" ) == 0 )
            fail( mode );

        using namespace E2ETestUtils;
        ControlChannel control{ Socket{ STDIN_FILENO } };
        control.sendResponse( ControlEvent::Ready, { { "height", uint64_t{ 1 } } },
            std::chrono::steady_clock::now() + std::chrono::seconds( 5 ) );
        const auto command = control.receiveCommand(
            std::chrono::steady_clock::now() + std::chrono::seconds( 5 ) );
        if ( mode == "progress-timeout" ) {
            for ( ;; )
                ::pause();
        }
        if ( mode == "stop-ack-exit" ) {
            if ( command != ControlCommand::Stop )
                throw std::runtime_error( "Expected stop command" );
            control.sendResponse( ControlEvent::Stopped, nlohmann::json::object(),
                std::chrono::steady_clock::now() + std::chrono::seconds( 5 ) );
        }
        fail( mode );
    } catch ( const std::exception& error ) {
        std::fprintf( stderr, "Consensus failure fixture failed: %s\n", error.what() );
        return 2;
    }
}
