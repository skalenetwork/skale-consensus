#include "ControlChannel.h"
#include "tests/harness/HarnessDiagnostics.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace E2ETestUtils {

ControlChannel::ControlChannel( Socket endpoint ) noexcept : socket( std::move( endpoint ) ) {}

void ControlChannel::requireOpen() const {
    if ( !socket )
        throw std::runtime_error( HarnessDiagnostics::CONTROL_CHANNEL_CLOSED );
}

void ControlChannel::close() noexcept {
    const int error = socket.reset();
    if ( error != 0 )
        std::fprintf( stderr, "Could not close test control socket: %s\n", std::strerror( error ) );
}

void ControlChannel::sendFrame( const std::string& body, Deadline deadline ) {
    requireOpen();
    if ( body.empty() || body.size() > MAX_FRAME_SIZE )
        throw std::runtime_error( "Invalid consensus control frame size" );
    const auto size = static_cast< uint32_t >( body.size() );
    sendAll( socket.get(), &size, sizeof( size ), deadline );
    sendAll( socket.get(), body.data(), body.size(), deadline );
}

std::string ControlChannel::receiveFrame( Deadline deadline ) {
    requireOpen();
    uint32_t size = 0;
    receiveExact( socket.get(), &size, sizeof( size ), deadline );
    if ( size == 0 || size > MAX_FRAME_SIZE )
        throw std::runtime_error( "Invalid consensus control frame size" );
    std::string body( size, '\0' );
    receiveExact( socket.get(), body.data(), body.size(), deadline );
    return body;
}

void ControlChannel::sendCommand( ControlCommand command, Deadline deadline ) {
    try {
        sendFrame( toString( command ), deadline );
    } catch ( ... ) {
        close();
        throw;
    }
}

ControlCommand ControlChannel::receiveCommand( Deadline deadline ) {
    try {
        return controlCommandFromString( receiveFrame( deadline ) );
    } catch ( ... ) {
        close();
        throw;
    }
}

void ControlChannel::sendResponse(
    ControlEvent event, nlohmann::json response, Deadline deadline ) {
    try {
        response["event"] = toString( event );
        sendFrame( response.dump(), deadline );
    } catch ( ... ) {
        close();
        throw;
    }
}

nlohmann::json ControlChannel::receiveResponse( ControlEvent expected, Deadline deadline ) {
    try {
        auto response = nlohmann::json::parse( receiveFrame( deadline ) );
        const auto event = controlEventFromString( response.at( "event" ).get< std::string >() );
        if ( event != expected )
            throw std::runtime_error( std::string( "Expected consensus child event: " ) +
                                      toString( expected ) );
        return response;
    } catch ( ... ) {
        // Partial frames and late responses cannot safely be reused.
        close();
        throw;
    }
}

} // namespace E2ETestUtils
