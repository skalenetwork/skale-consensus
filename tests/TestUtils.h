/*
    Copyright (C) 2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file TestUtils.h
    @author SKALE Labs
    @date 2026
*/

#pragma once

#include <memory>
#include <string>
#include <cstdlib>
#include <filesystem>

#include "SkaleCommon.h"
#include "chains/Schain.h"
#include "node/ConsensusEngine.h"
#include "node/Node.h"
#include "node/NodeInfo.h"
#include "tests/e2e/ConsensusEngineTestAccess.h"
#include "thirdparty/json.hpp"
#include "thirdparty/catch.hpp"

// RAII helper: creates a unique temporary directory for LevelDB files and
// redirects the engine's dbDir to it for the lifetime of the object.
// Prevents stale DB state from previous test runs sharing /tmp.
class TempDbDir {
public:
    explicit TempDbDir( ConsensusEngine& engine ) {
        char tmpl[] = "/tmp/consensus_test_XXXXXX";
        const char* created = mkdtemp( tmpl );
        CHECK_STATE2( created != nullptr, "mkdtemp failed" );
        path_ = created;
        ConsensusEngineTestAccess::setDbDir( engine, path_ );
    }

    ~TempDbDir() {
        if ( !path_.empty() ) {
            std::filesystem::remove_all( path_ );
        }
    }

    TempDbDir( const TempDbDir& ) = delete;
    TempDbDir& operator=( const TempDbDir& ) = delete;

private:
    std::string path_;
};

namespace TestUtils {

/**
 * @brief Creates a minimal test Node with the given configuration
 * 
 * @param cfg JSON configuration for the node
 * @param engine Pointer to the ConsensusEngine
 * @param gethUrl Optional geth URL (defaults to empty string)
 * @return std::shared_ptr<Node> The created Node
 */
inline std::shared_ptr<Node> createTestNode(
    const nlohmann::json& cfg,
    ConsensusEngine* engine,
    const std::string& gethUrl = "",
    bool isSyncNode = false) {
    auto mutableGethUrl = gethUrl;
    
    return std::make_shared<Node>(
        cfg, engine, false, "", "", "", "", nullptr, "", nullptr,
        nullptr, mutableGethUrl, nullptr, nullptr, nullptr, isSyncNode, false);
}

/**
 * @brief Creates a minimal test Schain with the given parameters
 * 
 * @param node The Node that owns this Schain
 * @param schainIndex The index of this Schain
 * @param schainId The ID of this Schain
 * @param schainName The name of this Schain
 * @return std::shared_ptr<Schain> The created Schain
 */
inline std::shared_ptr<Schain> createTestSchain(
    std::shared_ptr<Node> node,
    schain_index schainIndex,
    schain_id schainId,
    const std::string& schainName) {
    auto mutableSchainName = schainName;
    
    return std::make_shared<Schain>(node, schainIndex, schainId, nullptr, mutableSchainName);
}

/**
 * @brief Creates a complete test environment with Node and Schain
 * 
 * This is a convenience function that creates a Node with standard test configuration
 * and an associated Schain. It's useful for tests that need a basic consensus setup.
 * 
 * @param node_out Output parameter for the created Node
 * @param chain_out Output parameter for the created Schain
 * @param engine Reference to the ConsensusEngine
 * @param nodeId The node ID (defaults to 1)
 * @param nodeName The node name (defaults to "testNode")
 * @param bindIP The bind IP (defaults to "127.0.0.1")
 * @param basePort The base port (defaults to 10000)
 * @param schainId The schain ID (defaults to 1337)
 * @param schainName The schain name (defaults to "testChain")
 * @param schainIndex The schain index (defaults to 1)
 * @param gethUrl Optional geth URL (defaults to empty string)
 */
inline void createTestNodeAndSchain(
    std::shared_ptr<Node>& node_out,
    std::shared_ptr<Schain>& chain_out,
    ConsensusEngine& engine,
    node_id nodeId = node_id(1),
    const std::string& nodeName = "testNode",
    const std::string& bindIP = "127.0.0.1",
    uint16_t basePort = 10000,
    schain_id schainId = schain_id(1337),
    const std::string& schainName = "testChain",
    schain_index schainIndex = schain_index(1),
    const std::string& gethUrl = "") {
    
    // Create node configuration
    nlohmann::json cfg;
    cfg["nodeID"] = ( uint64_t ) nodeId;
    cfg["nodeName"] = nodeName;
    cfg["bindIP"] = bindIP;
    cfg["basePort"] = ( uint16_t ) basePort;
    
    // Create the node
    node_out = createTestNode(cfg, &engine, gethUrl);
    
    // Set node info
    auto nodeInfo = std::make_shared<NodeInfo>(nodeId, bindIP, basePort, schainId, schainIndex);
    node_out->setNodeInfo(nodeInfo);
    
    // Create the schain and register it on the node so that initLevelDBs() runs
    // and all database handles (including teDecryptionDB) are initialised.
    chain_out = createTestSchain(node_out, schainIndex, schainId, schainName);
    node_out->setSchain(chain_out);
    ConsensusEngineTestAccess::registerNode(engine, node_out);
}

}  // namespace TestUtils
