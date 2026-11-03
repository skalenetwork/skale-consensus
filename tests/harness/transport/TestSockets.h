#pragma once

#include "network/Socket.h"

#include <array>
#include <cerrno>
#include <sys/socket.h>
#include <system_error>

namespace E2ETestUtils {

inline Socket createTestSocket( int domain, int type, int protocol = 0 ) {
    const int fd = ::socket( domain, type | SOCK_CLOEXEC, protocol );
    if ( fd < 0 )
        throw std::system_error( errno, std::generic_category(), "Could not create test socket" );
    return Socket( fd );
}

inline std::array< Socket, 2 > createTestSocketPair() {
    int descriptors[2];
    if ( ::socketpair( AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, descriptors ) != 0 )
        throw std::system_error(
            errno, std::generic_category(), "Could not create test socket pair" );
    return { Socket( descriptors[0] ), Socket( descriptors[1] ) };
}

} // namespace E2ETestUtils
