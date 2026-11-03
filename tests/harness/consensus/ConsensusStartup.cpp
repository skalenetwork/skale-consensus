#include "ConsensusStartup.h"

#include <stdexcept>

namespace E2ETestUtils {

std::unique_ptr< ConsensusEngine > setupConsensusEngine( const ConsensusStartupOptions& _options ) {
    std::unique_ptr< ConsensusEngine > engine;
    if ( _options.externalFace != nullptr ) {
        if ( _options.lastCommittedBlockId < 0 )
            throw std::invalid_argument( "External-face startup requires a nonnegative block ID" );
        engine = std::make_unique< ConsensusEngine >( *_options.externalFace,
            static_cast< uint64_t >( _options.lastCommittedBlockId ),
            _options.lastCommittedTimestampS, _options.lastCommittedTimestampMs,
            _options.patchTimestamps, _options.storageLimitBytes );
    } else {
        engine = std::make_unique< ConsensusEngine >(
            _options.lastCommittedBlockId, _options.storageLimitBytes );
        if ( !_options.patchTimestamps.empty() )
            engine->setTestPatchTimestamps( _options.patchTimestamps );
    }

    const bool useBlockIdFromConsensus = _options.useBlockIdFromConsensus ||
                                        ( _options.externalFace == nullptr &&
                                            _options.lastCommittedBlockId == -1 );
    engine->parseTestConfigsAndCreateAllNodes(
        _options.configDirectory, useBlockIdFromConsensus );
    return engine;
}

std::unique_ptr< ConsensusEngine > startConsensusEngine( const ConsensusStartupOptions& _options ) {
    auto engine = setupConsensusEngine( _options );
    engine->slowStartBootStrapTest();
    return engine;
}

}  // namespace E2ETestUtils
