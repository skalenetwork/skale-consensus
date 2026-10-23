#pragma once

#include "SkaleCommon.h"
#include "node/ConsensusEngine.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace E2ETestUtils {

struct ConsensusStartupOptions {
    static constexpr uint64_t DEFAULT_STORAGE_LIMIT_BYTES = 1000000000;

    fs_path configDirectory;
    int64_t lastCommittedBlockId = -1;
    uint64_t storageLimitBytes = DEFAULT_STORAGE_LIMIT_BYTES;

    std::map< std::string, uint64_t > patchTimestamps = {
#ifdef BITE
        { "bite2PatchTimestamp", 0 }
#endif
    };

    // Borrowed; must outlive the returned engine.
    ConsensusExtFace* externalFace = nullptr;
    uint64_t lastCommittedTimestampS = 0;
    uint64_t lastCommittedTimestampMs = 0;
    bool useBlockIdFromConsensus = false;
};

// Constructs and configures nodes without starting services or bootstrapping.
std::unique_ptr< ConsensusEngine > setupConsensusEngine( const ConsensusStartupOptions& _options );

// Convenience wrapper for callers that do not need to wire dependencies before startup.
std::unique_ptr< ConsensusEngine > startConsensusEngine( const ConsensusStartupOptions& _options );

}  // namespace E2ETestUtils
