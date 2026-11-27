#include "SocketIO.h"
#include "tests/harness/HarnessDiagnostics.h"

#include <cerrno>
#include <cstring>
#include <limits>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>

namespace E2ETestUtils {
namespace {

template < typename Transfer >
void transferAll( int fd, size_t size, short events, Deadline deadline, Transfer transfer ) {
    size_t offset = 0;
    while ( offset < size ) {
        const auto now = std::chrono::steady_clock::now();
        if ( now >= deadline )
            throw std::runtime_error( HarnessDiagnostics::SOCKET_TIMEOUT );
        const auto remaining = std::chrono::duration_cast< std::chrono::milliseconds >(
            deadline - now ).count();
        pollfd event{ fd, events, 0 };
        const int timeout = remaining > std::numeric_limits< int >::max() ?
            std::numeric_limits< int >::max() : static_cast< int >( remaining );
        const int ready = ::poll( &event, 1, timeout );
        if ( ready < 0 && errno == EINTR )
            continue;
        if ( ready < 0 )
            throw std::runtime_error( std::string( "poll: " ) + std::strerror( errno ) );
        if ( ready == 0 )
            continue;
        if ( std::chrono::steady_clock::now() >= deadline )
            throw std::runtime_error( HarnessDiagnostics::SOCKET_TIMEOUT );
        const auto count = transfer( offset, size - offset );
        if ( count < 0 && ( errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK ) )
            continue;
        if ( count < 0 )
            throw std::runtime_error(
                std::string( "test socket transfer: " ) + std::strerror( errno ) );
        if ( count == 0 )
            throw std::runtime_error( "Consensus child or peer closed its connection" );
        offset += static_cast< size_t >( count );
    }
}

} // namespace

void sendAll( int fd, const void* data, size_t size, Deadline deadline ) {
    const auto* bytes = static_cast< const char* >( data );
    transferAll( fd, size, POLLOUT, deadline, [fd, bytes]( size_t offset, size_t remaining ) {
        return ::send( fd, bytes + offset, remaining, MSG_NOSIGNAL | MSG_DONTWAIT );
    } );
}

void receiveExact( int fd, void* data, size_t size, Deadline deadline ) {
    auto* bytes = static_cast< char* >( data );
    transferAll( fd, size, POLLIN, deadline, [fd, bytes]( size_t offset, size_t remaining ) {
        return ::recv( fd, bytes + offset, remaining, MSG_DONTWAIT );
    } );
}

} // namespace E2ETestUtils
