#include "tests/e2e/harness/SocketIO.h"
#include "thirdparty/catch.hpp"

using E2ETestUtils::deadlineIn;
using E2ETestUtils::WaitResult;
using E2ETestUtils::waitUntil;
using namespace std::chrono_literals;

CATCH_TEST_CASE( "Harness polling skips predicates after deadline expiry",
    "[unit][harness][deadline]" ) {
    bool called = false;
    const auto result = waitUntil(
        [&] {
            called = true;
            return true;
        },
        std::chrono::steady_clock::now(), 1ms );
    CATCH_REQUIRE( result == WaitResult::TimedOut );
    CATCH_REQUIRE_FALSE( called );
}

CATCH_TEST_CASE( "Harness polling stops at the first successful evaluation",
    "[unit][harness][deadline]" ) {
    int calls = 0;
    CATCH_REQUIRE( waitUntil( [&] { return ++calls == 3; }, deadlineIn( 1s ), 1ms ) ==
                   WaitResult::Satisfied );
    CATCH_REQUIRE( calls == 3 );
}

CATCH_TEST_CASE( "Harness polling times out when the predicate stays false",
    "[unit][harness][deadline]" ) {
    CATCH_REQUIRE( waitUntil( [] { return false; }, deadlineIn( 10ms ), 1ms ) ==
                   WaitResult::TimedOut );
}

CATCH_TEST_CASE( "Harness polling rejects success obtained after deadline expiry",
    "[unit][harness][deadline]" ) {
    const auto deadline = deadlineIn( 10ms );
    const auto result = waitUntil(
        [&] {
            std::this_thread::sleep_until( deadline );
            return true;
        },
        deadline, 1ms );
    CATCH_REQUIRE( result == WaitResult::TimedOut );
}

CATCH_TEST_CASE( "Harness polling preserves predicate failures",
    "[unit][harness][deadline]" ) {
    CATCH_REQUIRE_THROWS_WITH(
        waitUntil( []() -> bool { throw std::runtime_error( "Predicate failed" ); },
            deadlineIn( 1s ), 1ms ),
        "Predicate failed" );
}

CATCH_TEST_CASE( "Harness polling rejects nonpositive intervals",
    "[unit][harness][deadline]" ) {
    CATCH_REQUIRE_THROWS_AS(
        waitUntil( [] { return true; }, deadlineIn( 1s ), 0ms ), std::invalid_argument );
    CATCH_REQUIRE_THROWS_AS(
        waitUntil( [] { return true; }, deadlineIn( 1s ), -1ms ), std::invalid_argument );
}
