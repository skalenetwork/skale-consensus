#include "SkaleCommon.h"
#include "tests/e2e/harness/ControlChannel.h"
#include "tests/e2e/harness/TestSockets.h"
#include "thirdparty/catch.hpp"

#include <thread>

CATCH_TEST_CASE( "Consensus control tags round trip and reject unknown values",
    "[unit][harness][protocol]" ) {
    using namespace E2ETestUtils;
    CATCH_REQUIRE( std::string( toString( ControlCommand::Progress ) ) == "progress" );
    CATCH_REQUIRE( std::string( toString( ControlCommand::Stop ) ) == "stop" );
    CATCH_REQUIRE(
        std::string( toString( ControlCommand::InjectAcceptError ) ) == "inject-accept-error" );
    CATCH_REQUIRE(
        std::string( toString( ControlCommand::ClearAcceptError ) ) == "clear-accept-error" );
    CATCH_REQUIRE(
        std::string( toString( ControlCommand::AcceptErrorCount ) ) == "accept-error-count" );
    CATCH_REQUIRE( std::string( toString( ControlEvent::Ready ) ) == "ready" );
    CATCH_REQUIRE( std::string( toString( ControlEvent::Progress ) ) == "progress" );
    CATCH_REQUIRE( std::string( toString( ControlEvent::Stopped ) ) == "stopped" );
    CATCH_REQUIRE(
        std::string( toString( ControlEvent::AcceptErrorEnabled ) ) == "accept-error-enabled" );
    CATCH_REQUIRE(
        std::string( toString( ControlEvent::AcceptErrorCleared ) ) == "accept-error-cleared" );
    CATCH_REQUIRE(
        std::string( toString( ControlEvent::AcceptErrorCount ) ) == "accept-error-count" );
    for ( auto command : { ControlCommand::Progress, ControlCommand::Stop,
             ControlCommand::InjectAcceptError, ControlCommand::ClearAcceptError,
             ControlCommand::AcceptErrorCount } )
        CATCH_REQUIRE( controlCommandFromString( toString( command ) ) == command );
    for ( auto event : { ControlEvent::Ready, ControlEvent::Progress, ControlEvent::Stopped,
             ControlEvent::AcceptErrorEnabled, ControlEvent::AcceptErrorCleared,
             ControlEvent::AcceptErrorCount } )
        CATCH_REQUIRE( controlEventFromString( toString( event ) ) == event );
    CATCH_REQUIRE_THROWS_AS( controlCommandFromString( "unknown" ), std::invalid_argument );
    CATCH_REQUIRE_THROWS_AS( controlEventFromString( "unknown" ), std::invalid_argument );
    CATCH_REQUIRE_THROWS_AS( controlCommandFromString( "" ), std::invalid_argument );
    CATCH_REQUIRE_THROWS_AS( controlEventFromString( "" ), std::invalid_argument );
    CATCH_REQUIRE_THROWS_WITH(
        toString( static_cast< ControlCommand >( -1 ) ), "Invalid consensus control command" );
    CATCH_REQUIRE_THROWS_WITH(
        toString( static_cast< ControlEvent >( -1 ) ), "Invalid consensus control event" );
}

CATCH_TEST_CASE( "Control channels exchange typed commands and JSON responses",
    "[unit][harness][protocol]" ) {
    using namespace E2ETestUtils;
    auto sockets = createTestSocketPair();
    ControlChannel parent{ std::move( sockets[0] ) };
    ControlChannel child{ std::move( sockets[1] ) };
    const auto deadline = deadlineIn( std::chrono::seconds( 1 ) );

    child.sendResponse( ControlEvent::Ready, { { "height", uint64_t{ 1 } } }, deadline );
    CATCH_REQUIRE( parent.receiveResponse( ControlEvent::Ready, deadline ).at( "height" ) == 1 );
    parent.sendCommand( ControlCommand::Progress, deadline );
    CATCH_REQUIRE( child.receiveCommand( deadline ) == ControlCommand::Progress );
    child.sendResponse( ControlEvent::Progress, { { "height", uint64_t{ 2 } } }, deadline );
    CATCH_REQUIRE( parent.receiveResponse( ControlEvent::Progress, deadline ).at( "height" ) == 2 );
    parent.sendCommand( ControlCommand::Stop, deadline );
    CATCH_REQUIRE( child.receiveCommand( deadline ) == ControlCommand::Stop );
    child.sendResponse( ControlEvent::Stopped, nlohmann::json::object(), deadline );
    CATCH_REQUIRE( parent.receiveResponse( ControlEvent::Stopped, deadline ).size() == 1 );
}

CATCH_TEST_CASE( "Control channels reject invalid responses and cannot be reused",
    "[unit][harness][protocol]" ) {
    using namespace E2ETestUtils;
    auto sockets = createTestSocketPair();
    ControlChannel channel{ std::move( sockets[0] ) };
    const auto deadline = deadlineIn( std::chrono::seconds( 1 ) );
    std::string body;
    uint32_t size = 0;

    CATCH_SECTION( "Empty frame" ) {}
    CATCH_SECTION( "Oversized frame" ) { size = ControlChannel::MAX_FRAME_SIZE + 1; }
    CATCH_SECTION( "Malformed JSON" ) { body = "{"; }
    CATCH_SECTION( "Missing event" ) { body = "{}"; }
    CATCH_SECTION( "Unknown event" ) { body = R"({"event":"unknown"})"; }
    CATCH_SECTION( "Unexpected event" ) { body = R"({"event":"stopped"})"; }

    if ( !body.empty() )
        size = static_cast< uint32_t >( body.size() );
    sendAll( sockets[1].get(), &size, sizeof( size ), deadline );
    if ( !body.empty() )
        sendAll( sockets[1].get(), body.data(), body.size(), deadline );
    CATCH_REQUIRE_THROWS( channel.receiveResponse( ControlEvent::Progress, deadline ) );
    CATCH_REQUIRE_THROWS_WITH( channel.sendCommand( ControlCommand::Progress, deadline ),
        "Consensus child control channel is closed" );
}

CATCH_TEST_CASE( "Control channels close after incomplete frames reach their deadline",
    "[unit][harness][protocol][deadline]" ) {
    using namespace E2ETestUtils;
    auto sockets = createTestSocketPair();
    ControlChannel channel{ std::move( sockets[0] ) };
    const auto deadline = deadlineIn( std::chrono::milliseconds( 50 ) );
    const uint32_t size = 8;
    sendAll( sockets[1].get(), &size, sizeof( size ), deadline );
    sendAll( sockets[1].get(), "pro", 3, deadline );
    CATCH_REQUIRE_THROWS_WITH( channel.receiveCommand( deadline ),
        "Consensus test socket operation timed out" );
    CATCH_REQUIRE_THROWS_WITH( channel.receiveCommand( deadline ),
        "Consensus child control channel is closed" );
}

CATCH_TEST_CASE( "Control channels reject unknown commands",
    "[unit][harness][protocol]" ) {
    using namespace E2ETestUtils;
    auto sockets = createTestSocketPair();
    ControlChannel channel{ std::move( sockets[0] ) };
    const auto deadline = deadlineIn( std::chrono::seconds( 1 ) );
    const std::string body = "unknown";
    const auto size = static_cast< uint32_t >( body.size() );
    sendAll( sockets[1].get(), &size, sizeof( size ), deadline );
    sendAll( sockets[1].get(), body.data(), body.size(), deadline );
    CATCH_REQUIRE_THROWS_WITH( channel.receiveCommand( deadline ), "Unknown parent command: unknown" );
    CATCH_REQUIRE_THROWS_WITH( channel.receiveCommand( deadline ),
        "Consensus child control channel is closed" );
}

CATCH_TEST_CASE( "Control responses cannot reset an exchange deadline",
    "[unit][harness][protocol][deadline]" ) {
    using namespace E2ETestUtils;
    auto sockets = createTestSocketPair();
    ControlChannel parent{ std::move( sockets[0] ) };
    ControlChannel child{ std::move( sockets[1] ) };
    const auto deadline = deadlineIn( std::chrono::milliseconds( 50 ) );
    parent.sendCommand( ControlCommand::Progress, deadline );
    CATCH_REQUIRE( child.receiveCommand( deadline ) == ControlCommand::Progress );
    child.sendResponse( ControlEvent::Progress, { { "height", uint64_t{ 1 } } }, deadline );
    std::this_thread::sleep_until( deadline );
    CATCH_REQUIRE_THROWS_WITH( parent.receiveResponse( ControlEvent::Progress, deadline ),
        "Consensus test socket operation timed out" );
    CATCH_REQUIRE_THROWS_WITH( parent.sendCommand( ControlCommand::Progress, deadline ),
        "Consensus child control channel is closed" );
}

CATCH_TEST_CASE( "Consensus socket transfers respect shared deadlines",
    "[unit][harness][socket-io][deadline]" ) {
    using E2ETestUtils::sendAll;
    using E2ETestUtils::receiveExact;
    auto sockets = E2ETestUtils::createTestSocketPair();
    char sent = 'x';
    const auto deadline = E2ETestUtils::deadlineIn( std::chrono::milliseconds( 50 ) );
    sendAll( sockets[0].get(), &sent, sizeof( sent ), deadline );

    CATCH_SECTION( "An expired deadline rejects a subsequent transfer even with data ready" ) {
        std::this_thread::sleep_until( deadline );
        char received = '\0';
        CATCH_REQUIRE_THROWS_WITH(
            receiveExact( sockets[1].get(), &received, sizeof( received ), deadline ),
            "Consensus test socket operation timed out" );
        CATCH_REQUIRE( received == '\0' );
    }

    CATCH_SECTION( "A partial frame cannot extend the deadline" ) {
        char received[2]{};
        CATCH_REQUIRE_THROWS_WITH(
            receiveExact( sockets[1].get(), received, sizeof( received ), deadline ),
            "Consensus test socket operation timed out" );
        CATCH_REQUIRE( received[0] == sent );
        CATCH_REQUIRE( std::chrono::steady_clock::now() < deadline + std::chrono::seconds( 1 ) );
    }
}
