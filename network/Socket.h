#pragma once

#include <cerrno>
#include <unistd.h>
#include <utility>

class Socket {
    int descriptor = -1;

public:
    Socket() noexcept = default;
    explicit Socket( int ownedDescriptor ) noexcept : descriptor( ownedDescriptor ) {}
    ~Socket() { (void) reset(); }

    Socket( const Socket& ) = delete;
    Socket& operator=( const Socket& ) = delete;

    Socket( Socket&& other ) noexcept
        : descriptor( std::exchange( other.descriptor, -1 ) ) {}

    Socket& operator=( Socket&& other ) noexcept {
        if ( this != &other ) {
            (void) reset();
            descriptor = std::exchange( other.descriptor, -1 );
        }
        return *this;
    }

    int get() const noexcept { return descriptor; }
    explicit operator bool() const noexcept { return descriptor >= 0; }
    int release() noexcept { return std::exchange( descriptor, -1 ); }

    // Returns a close errno, or zero on success. Destruction is best-effort and does no logging.
    [[nodiscard]] int reset() noexcept {
        const int ownedDescriptor = std::exchange( descriptor, -1 );
        if ( ownedDescriptor < 0 )
            return 0;
        const int savedErrno = errno;
        // On Linux, close releases the descriptor even on EINTR; retrying risks closing a reused fd.
        const int error = ::close( ownedDescriptor ) < 0 ? errno : 0;
        errno = savedErrno;
        return error;
    }
};
