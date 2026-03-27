#include "thirdparty/catch.hpp"

#include <cerrno>
#include <exception>

#include "exceptions/InvalidStateException.h"
#include "exceptions/NetworkProtocolException.h"
#include "exceptions/ParsingException.h"
#include "exceptions/ZMQTransportException.h"
#include "network/ZMQErrorClassifier.h"
#include "zmq.h"

CATCH_TEST_CASE( "ZMQ transport fallback classifier accepts transport errno values",
    "[network][zmq][unit]" ) {
    CATCH_REQUIRE( isZMQTransportErrorForTcpFallback( EAGAIN ) );
    CATCH_REQUIRE( isZMQTransportErrorForTcpFallback( ETIMEDOUT ) );
    CATCH_REQUIRE( isZMQTransportErrorForTcpFallback( ECONNRESET ) );
    CATCH_REQUIRE( isZMQTransportErrorForTcpFallback( ECONNREFUSED ) );
    CATCH_REQUIRE( isZMQTransportErrorForTcpFallback( ENETDOWN ) );
    CATCH_REQUIRE( isZMQTransportErrorForTcpFallback( ENETUNREACH ) );
    CATCH_REQUIRE( isZMQTransportErrorForTcpFallback( EHOSTUNREACH ) );
    CATCH_REQUIRE( isZMQTransportErrorForTcpFallback( EPIPE ) );
}

CATCH_TEST_CASE( "ZMQ transport fallback classifier rejects non-transport errno values",
    "[network][zmq][unit]" ) {
    CATCH_REQUIRE_FALSE( isZMQTransportErrorForTcpFallback( EINVAL ) );
    CATCH_REQUIRE_FALSE( isZMQTransportErrorForTcpFallback( EFSM ) );
    CATCH_REQUIRE_FALSE( isZMQTransportErrorForTcpFallback( ETERM ) );
}

CATCH_TEST_CASE( "BlockFinalize TCP fallback only applies to ZMQ transport exceptions",
    "[blockfinalize][network][unit]" ) {
    ZMQTransportException transportException( "transport", EAGAIN, "Test" );
    ParsingException parsingException( "parsing", "Test" );
    NetworkProtocolException protocolException( "protocol", "Test" );
    InvalidStateException invalidStateException( "state", "Test" );
    auto catchesAsTransport = []( const auto& _exception ) {
        try {
            throw _exception;
        } catch ( const ZMQTransportException& ) {
            return true;
        } catch ( ... ) {
            return false;
        }
    };

    CATCH_REQUIRE( transportException.getZmqErrno() == EAGAIN );
    CATCH_REQUIRE( catchesAsTransport( transportException ) );
    CATCH_REQUIRE_FALSE( catchesAsTransport( parsingException ) );
    CATCH_REQUIRE_FALSE( catchesAsTransport( protocolException ) );
    CATCH_REQUIRE_FALSE( catchesAsTransport( invalidStateException ) );
}
