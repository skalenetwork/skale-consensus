#include "thirdparty/catch.hpp"

#include "tests/TestUtils.h"
#include "tests/unit/network/NodeTestAccess.h"
#include "network/Sockets.h"
#include "network/ZMQSockets.h"
#include "network/TCPServerSocket.h"

#include <array>
#include <unistd.h>

#include "zmq.h"

namespace {

class SocketsCleanupGuard {
    Sockets& sockets;

public:
    explicit SocketsCleanupGuard( Sockets& _sockets ) : sockets( _sockets ) {}

    ~SocketsCleanupGuard() {
        if ( sockets.consensusZMQSockets ) {
            sockets.consensusZMQSockets->closeAndCleanupAll();
        }
        if ( sockets.bulkDataZMQSockets ) {
            sockets.bulkDataZMQSockets->closeAndCleanupAll();
        }
        if ( sockets.blockProposalSocket ) {
            sockets.blockProposalSocket->closeAndCleanupAll();
        }
        if ( sockets.catchupSocket ) {
            sockets.catchupSocket->closeAndCleanupAll();
        }
    }
};

}  // namespace

CATCH_TEST_CASE( "Bulk data ZMQ bind uses overridden port delta", "[network][zmq][unit]" ) {
    constexpr uint16_t overrideDelta = 37;
    const auto basePort = static_cast< uint16_t >( 32000 + ( getpid() % 1000 ) * 20 );

    nlohmann::json cfg;
    cfg["nodeID"] = 1;
    cfg["nodeName"] = "testNode";
    cfg["bindIP"] = "127.0.0.1";
    cfg["basePort"] = basePort;
    cfg["ZMQBulkDataPortDelta"] = overrideDelta;

    ConsensusEngine engine( 0, 1000000000 );
    auto node = TestUtils::createTestNode( cfg, &engine, "http://127.0.0.1:8545" );

    const auto& portDeltas = NodeTestAccess::getSocketPortDeltas( *node );
    CATCH_REQUIRE( portDeltas.ZMQBulkDataPortDelta == overrideDelta );

    Sockets sockets( *node );
    sockets.initSockets(
        node->getBindIP(), static_cast< uint16_t >( node->getBasePort() ), portDeltas );
    SocketsCleanupGuard cleanup( sockets );

    auto bulkDataSocket = sockets.getBulkDataZMQSockets();
    void* receiveSocket = nullptr;
    CATCH_REQUIRE_NOTHROW( receiveSocket = bulkDataSocket->getReceiveSocket() );
    CATCH_REQUIRE( receiveSocket != nullptr );

    std::array< char, 256 > endpoint{};
    size_t endpointSize = endpoint.size();
    CATCH_REQUIRE( zmq_getsockopt(
                       receiveSocket, ZMQ_LAST_ENDPOINT, endpoint.data(), &endpointSize ) == 0 );

    const auto expectedEndpoint =
        string( "tcp://127.0.0.1:" ) + to_string( node->getBasePort() + overrideDelta );
    CATCH_REQUIRE( string( endpoint.data() ) == expectedEndpoint );
}
