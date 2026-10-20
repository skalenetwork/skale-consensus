#include "utils/LogThrottle.h"
#include "thirdparty/catch.hpp"

using namespace std::chrono;

CATCH_TEST_CASE( "LogThrottle rejects nonpositive intervals", "[unit][utils][log-throttle]" ) {
    CATCH_REQUIRE_THROWS_AS( LogThrottle( seconds( 0 ) ), std::invalid_argument );
    CATCH_REQUIRE_THROWS_AS( LogThrottle( seconds( -1 ) ), std::invalid_argument );
    CATCH_REQUIRE_NOTHROW( LogThrottle( steady_clock::duration( 1 ) ) );
}

CATCH_TEST_CASE( "LogThrottle reports the first event immediately", "[unit][utils][log-throttle]" ) {
    LogThrottle throttle( seconds( 5 ) );
    const steady_clock::time_point start{};

    CATCH_REQUIRE( throttle.lastReportEventCount() == 0 );
    CATCH_REQUIRE( throttle.shouldLog( start ) );
    CATCH_REQUIRE( throttle.lastReportEventCount() == 1 );
    CATCH_REQUIRE_FALSE( throttle.shouldLog( start ) );
    CATCH_REQUIRE( throttle.lastReportEventCount() == 1 );
}

CATCH_TEST_CASE( "LogThrottle reports at the exact interval boundary",
    "[unit][utils][log-throttle]" ) {
    LogThrottle throttle( seconds( 5 ) );
    const steady_clock::time_point start{};

    CATCH_REQUIRE( throttle.shouldLog( start ) );
    CATCH_REQUIRE_FALSE(
        throttle.shouldLog( start + seconds( 5 ) - steady_clock::duration( 1 ) ) );
    CATCH_REQUIRE( throttle.shouldLog( start + seconds( 5 ) ) );
    CATCH_REQUIRE( throttle.lastReportEventCount() == 2 );
    CATCH_REQUIRE_FALSE( throttle.shouldLog( start + seconds( 5 ) ) );
}

CATCH_TEST_CASE( "LogThrottle counts suppressed events and resets each reporting window",
    "[unit][utils][log-throttle]" ) {
    LogThrottle throttle( seconds( 5 ) );
    const steady_clock::time_point start{};

    CATCH_REQUIRE( throttle.shouldLog( start ) );
    CATCH_REQUIRE_FALSE( throttle.shouldLog( start + seconds( 1 ) ) );
    CATCH_REQUIRE_FALSE( throttle.shouldLog( start + seconds( 2 ) ) );
    CATCH_REQUIRE( throttle.shouldLog( start + seconds( 5 ) ) );
    CATCH_REQUIRE( throttle.lastReportEventCount() == 3 );

    CATCH_REQUIRE_FALSE( throttle.shouldLog( start + seconds( 6 ) ) );
    CATCH_REQUIRE( throttle.lastReportEventCount() == 3 );
    CATCH_REQUIRE( throttle.shouldLog( start + seconds( 10 ) ) );
    CATCH_REQUIRE( throttle.lastReportEventCount() == 2 );
    CATCH_REQUIRE( throttle.shouldLog( start + seconds( 15 ) ) );
    CATCH_REQUIRE( throttle.lastReportEventCount() == 1 );
}

CATCH_TEST_CASE( "LogThrottle starts the next interval at the actual report time",
    "[unit][utils][log-throttle]" ) {
    LogThrottle throttle( seconds( 5 ) );
    const steady_clock::time_point start{};

    CATCH_REQUIRE( throttle.shouldLog( start ) );
    CATCH_REQUIRE_FALSE( throttle.shouldLog( start + seconds( 1 ) ) );
    CATCH_REQUIRE( throttle.shouldLog( start + seconds( 12 ) ) );
    CATCH_REQUIRE( throttle.lastReportEventCount() == 2 );
    CATCH_REQUIRE_FALSE( throttle.shouldLog( start + seconds( 15 ) ) );
    CATCH_REQUIRE( throttle.shouldLog( start + seconds( 17 ) ) );
    CATCH_REQUIRE( throttle.lastReportEventCount() == 2 );
}

CATCH_TEST_CASE( "LogThrottle instances have independent intervals and counts",
    "[unit][utils][log-throttle]" ) {
    LogThrottle first( seconds( 2 ) );
    LogThrottle second( seconds( 5 ) );
    const steady_clock::time_point start{};

    CATCH_REQUIRE( first.shouldLog( start ) );
    CATCH_REQUIRE( second.shouldLog( start ) );
    CATCH_REQUIRE( first.shouldLog( start + seconds( 2 ) ) );
    CATCH_REQUIRE_FALSE( second.shouldLog( start + seconds( 2 ) ) );
    CATCH_REQUIRE( first.lastReportEventCount() == 1 );
    CATCH_REQUIRE( second.shouldLog( start + seconds( 5 ) ) );
    CATCH_REQUIRE( second.lastReportEventCount() == 2 );
}

CATCH_TEST_CASE( "LogThrottle supports the default clock entry point",
    "[unit][utils][log-throttle]" ) {
    LogThrottle throttle( seconds( 5 ) );

    CATCH_REQUIRE( throttle.shouldLog() );
    CATCH_REQUIRE( throttle.lastReportEventCount() == 1 );
}
