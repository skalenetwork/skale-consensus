#include "thirdparty/catch.hpp"

#include "SkaleCommon.h"
#include "tests/TestUtils.h"

namespace {

nlohmann::json makeTestNodeConfig() {
    nlohmann::json cfg;
    cfg["nodeID"] = 1;
    cfg["nodeName"] = "testNode";
    cfg["bindIP"] = "127.0.0.1";
    cfg["basePort"] = 31000;
    return cfg;
}

}  // namespace

CATCH_TEST_CASE( "BlockFinalize TCP fallback timeout uses config override",
    "[network][node][unit]" ) {
    constexpr uint64_t overrideTimeoutMs = 4242;
    TestUtils::TestEnvVarGuard envGuard( "blockFinalizeDownloadTcpFallbackMs" );

    auto cfg = makeTestNodeConfig();
    cfg["blockFinalizeDownloadTcpFallbackMs"] = overrideTimeoutMs;

    ConsensusEngine engine( 0, 1000000000 );
    auto node = TestUtils::createTestNode( cfg, &engine, "http://127.0.0.1:8545" );

    CATCH_REQUIRE( node->getBlockFinalizeDownloadTcpFallbackMs() == overrideTimeoutMs );
}

CATCH_TEST_CASE( "BlockFinalize TCP fallback timeout uses default when config is unset",
    "[network][node][unit]" ) {
    TestUtils::TestEnvVarGuard envGuard( "blockFinalizeDownloadTcpFallbackMs" );

    auto cfg = makeTestNodeConfig();

    ConsensusEngine engine( 0, 1000000000 );
    auto node = TestUtils::createTestNode( cfg, &engine, "http://127.0.0.1:8545" );

    CATCH_REQUIRE(
        node->getBlockFinalizeDownloadTcpFallbackMs() ==
        BLOCK_FINALIZE_DOWNLOAD_TCP_FALLBACK_MS );
}
