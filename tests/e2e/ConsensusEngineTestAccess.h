// tests/ConsensusEngineTestAccess.h
#pragma once
#include "node/ConsensusEngine.h"
#include "chains/Schain.h"
#include "db/BlockDB.h"

class ConsensusEngineTestAccess {
public:
    static ptr<CommittedBlock> getCommittedBlockForBlockId(
        const ConsensusEngine& e, uint64_t id) {

        CHECK_STATE( e.nodes.size() > 0 );

        for ( auto&& item : e.nodes ) {
            CHECK_STATE( item.second );
            auto schain = item.second->getSchain();
            CHECK_STATE( schain );
            CHECK_STATE( id <= schain->readLastCommittedBlockIDFromDb() );

            auto block = item.second->getBlockDB()->getBlock( id, schain->getCryptoManager() );
            CHECK_STATE( block );
            return block;
        }
        return nullptr;  // make compiler happy
    }

    static ptr<CommittedBlock> getCommittedBlockForBlockIdForNode(
        const ConsensusEngine& e, uint64_t id, node_id node) {

        CHECK_STATE( e.nodes.size() >= 1 );

        auto it = e.nodes.find( node );
        CHECK_STATE2( it != e.nodes.end(), "Node with id " + to_string( node ) + " not found" );

        CHECK_STATE( it->second );
        auto schain = it->second->getSchain();
        CHECK_STATE( schain );
        CHECK_STATE( id <= schain->readLastCommittedBlockIDFromDb() );

        auto block = it->second->getBlockDB()->getBlock( id, schain->getCryptoManager() );
        CHECK_STATE( block );
        return block;

    }
};
