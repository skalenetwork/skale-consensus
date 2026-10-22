#include "SkaleCommon.h"
#include "abstracttcpserver/ConnectionStatus.h"
#include "headers/BasicHeader.h"
#include "tests/harness/process/ConsensusProcess.h"
#include "tests/harness/transport/TestSockets.h"
#include "thirdparty/catch.hpp"

#include <arpa/inet.h>
#include <algorithm>
#include <fstream>
#include <limits>
#include <poll.h>
#include <thread>

namespace {
using E2ETestUtils::ConsensusProcess;
using E2ETestUtils::Deadline;
using E2ETestUtils::WaitResult;
using E2ETestUtils::deadlineIn;
using E2ETestUtils::waitUntil;
using E2ETestUtils::receiveExact;
using E2ETestUtils::sendAll;

Socket connectCatchup( const ConsensusProcess& child, Deadline deadline ) {
    auto socket = E2ETestUtils::createTestSocket( AF_INET, SOCK_STREAM | SOCK_NONBLOCK );
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons( ConsensusProcess::BASE_PORT + port_type::CATCHUP );
    CATCH_REQUIRE( ::inet_pton( AF_INET, child.address().c_str(), &address.sin_addr ) == 1 );
    const auto connected =
        ::connect( socket.get(), reinterpret_cast< sockaddr* >( &address ), sizeof( address ) );
    if ( connected < 0 ) {
        CATCH_REQUIRE( errno == EINPROGRESS );
        pollfd event{ socket.get(), POLLOUT, 0 };
        int result;
        do {
            const auto remaining = std::chrono::duration_cast< std::chrono::milliseconds >(
                deadline - std::chrono::steady_clock::now() )
                                       .count();
            CATCH_REQUIRE( remaining > 0 );
            result = ::poll( &event, 1, static_cast< int >( remaining ) );
        } while ( result < 0 && errno == EINTR );
        CATCH_REQUIRE( result == 1 );
        int error = 0;
        socklen_t length = sizeof( error );
        CATCH_REQUIRE(
            ::getsockopt( socket.get(), SOL_SOCKET, SO_ERROR, &error, &length ) == 0 );
        CATCH_REQUIRE( error == 0 );
    }
    return socket;
}

void sendCatchupRequest( int fd, Deadline deadline ) {
    // Ask beyond the current height for a valid, header-only "no new blocks" response.
    nlohmann::json request{ { "type", BasicHeader::BLOCK_CATCHUP_REQ }, { "schainID", 1 },
        { "nodeID", 1 }, { "blockID", std::numeric_limits< uint64_t >::max() } };
    auto body = request.dump();
    uint64_t magic = MAGIC_NUMBER;
    uint64_t length = body.size();
    sendAll( fd, &magic, sizeof( magic ), deadline );
    sendAll( fd, &length, sizeof( length ), deadline );
    sendAll( fd, body.data(), body.size(), deadline );
}

void requireCatchupAvailable( const ConsensusProcess& child ) {
    const auto catchupDeadline = deadlineIn( std::chrono::seconds( 10 ) );
    auto socket = connectCatchup( child, catchupDeadline );
    sendCatchupRequest( socket.get(), catchupDeadline );
    uint64_t length = 0;
    receiveExact( socket.get(), &length, sizeof( length ), catchupDeadline );
    CATCH_REQUIRE( length >= 2 );
    CATCH_REQUIRE( length <= MAX_HEADER_SIZE );
    std::string body( length, '\0' );
    receiveExact( socket.get(), body.data(), body.size(), catchupDeadline );
    const auto response = nlohmann::json::parse( body );
    CATCH_REQUIRE(
        response.at( "type" ).get< std::string >() == BasicHeader::BLOCK_CATCHUP_RSP );
    CATCH_REQUIRE( response.at( "status" ).get< int >() == CONNECTION_DISCONNECT );
    CATCH_REQUIRE( response.at( "substatus" ).get< int >() == CONNECTION_NO_NEW_BLOCKS );
}

bool childLogContains( const ConsensusProcess& child, const std::string& message ) {
    const auto path = child.logsDirectory() / "skaled.log";
    std::ifstream input( path );
    if ( !input )
        throw std::runtime_error( "Could not read child log: " + path.string() );
    std::string line;
    while ( std::getline( input, line ) ) {
        if ( line.find( message ) != std::string::npos )
            return true;
    }
    if ( input.bad() )
        throw std::runtime_error( "Failed reading child log: " + path.string() );
    return false;
}

void requireLogMessage(
    const ConsensusProcess& child, const std::string& message, Deadline deadline ) {
    CATCH_INFO( "Waiting for child log message: " << message
                << "; log: " << ( child.logsDirectory() / "skaled.log" ).string() );
    const auto result = waitUntil( [&] { return childLogContains( child, message ); }, deadline,
        std::chrono::milliseconds( 50 ) );
    if ( result == WaitResult::TimedOut )
        CATCH_FAIL( "Timed out waiting for child log message" );
}

void requireConsensusProgress( ConsensusProcess& child, uint64_t previous, Deadline deadline ) {
    uint64_t height = previous;
    const auto result = waitUntil(
        [&] {
            CATCH_INFO( "Waiting for consensus height above " << previous
                        << "; last observed height: " << height );
            CATCH_REQUIRE_NOTHROW( height = child.progress( deadline ) );
            return height > previous;
        },
        deadline, std::chrono::milliseconds( 100 ) );
    CATCH_INFO( "Waiting for consensus height above " << previous
                << "; last observed height: " << height );
    if ( result == WaitResult::TimedOut )
        CATCH_FAIL( "Timed out waiting for consensus progress" );
}

uint64_t requireAcceptRetries( ConsensusProcess& child, uint64_t minimum, Deadline deadline ) {
    uint64_t retries = 0;
    const auto result = waitUntil(
        [&] {
            retries = child.acceptErrorCount( deadline );
            return retries >= minimum;
        },
        deadline, std::chrono::milliseconds( 50 ) );
    CATCH_INFO( "Expected at least " << minimum << " accept retries; observed: " << retries );
    CATCH_REQUIRE( result == WaitResult::Satisfied );
    return retries;
}
}  // namespace

CATCH_TEST_CASE( "Consensus process retries and recovers from injected TCP accept exhaustion",
    "[end-to-end][consensus-process][descriptor-exhaustion]" ) {
    ConsensusProcess child{ CONSENSUS_TEST_CHILD_PATH, CONSENSUS_TEST_SOURCE_ROOT };
    child.start();
    CATCH_REQUIRE_THROWS_AS( child.start(), std::logic_error );
    const auto initialHeight = child.progress();
    CATCH_REQUIRE( initialHeight > 0 );

    requireCatchupAvailable( child );
    CATCH_REQUIRE_FALSE( childLogContains( child, "CatchupServer: accept resource exhaustion" ) );
    child.injectAcceptError( deadlineIn( std::chrono::seconds( 10 ) ) );
    uint64_t finalCount = 0;
    {
        // Wake a real accept() that may already have passed the hook before activation.
        const auto exhaustionDeadline = deadlineIn( std::chrono::seconds( 10 ) );
        auto pending = connectCatchup( child, exhaustionDeadline );
        sendCatchupRequest( pending.get(), exhaustionDeadline );
        const auto retries = requireAcceptRetries( child, 3, exhaustionDeadline );
        requireLogMessage(
            child, "CatchupServer: accept resource exhaustion; retrying", exhaustionDeadline );
        const auto heightDuringExhaustion = child.progress();
        requireConsensusProgress( child, heightDuringExhaustion,
            deadlineIn( std::chrono::seconds( 20 ) ) );
        child.clearAcceptError( deadlineIn( std::chrono::seconds( 10 ) ) );
        finalCount = child.acceptErrorCount( deadlineIn( std::chrono::seconds( 10 ) ) );
        CATCH_REQUIRE( finalCount >= retries );
    }
    const auto heightBeforeRecoveryCheck = child.progress();
    requireCatchupAvailable( child );
    requireLogMessage( child, "CatchupServer: TCP acceptance recovered from resource exhaustion",
        deadlineIn( std::chrono::seconds( 10 ) ) );

    requireConsensusProgress( child, heightBeforeRecoveryCheck,
        deadlineIn( std::chrono::seconds( 20 ) ) );
    CATCH_REQUIRE( child.acceptErrorCount( deadlineIn( std::chrono::seconds( 10 ) ) ) == finalCount );
    child.stop();
    CATCH_REQUIRE_THROWS_AS( child.start(), std::logic_error );
    CATCH_REQUIRE_THROWS_AS( child.progress(), std::logic_error );
    CATCH_REQUIRE_THROWS_AS( child.stop(), std::logic_error );
}
