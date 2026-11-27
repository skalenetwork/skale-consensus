// tests/ConsensusEngineTestAccess.h
#pragma once
#include "node/ConsensusEngine.h"
#include "chains/Schain.h"
#include "db/BlockDB.h"
#include "blockproposal/pusher/BlockProposalClientAgent.h"
#include "datastructures/BlockProposal.h"
#include <initializer_list>
#ifdef BITE
#include "db/TEDecryptionDB.h"
#endif

class ConsensusEngineTestAccess {
public:
    static void registerNode( ConsensusEngine& e, const ptr< Node >& node ) {
        CHECK_STATE( node );
        CHECK_STATE( e.nodes.count( node->getNodeID() ) == 0 );

        e.nodes[node->getNodeID()] = node;
        e.nodeIDs.insert( node->getNodeID() );
    }

    static node_id getFirstNodeId( const ConsensusEngine& e ) {
        CHECK_STATE( e.nodes.size() > 0 );
        auto it = e.nodes.begin();
        CHECK_STATE( it != e.nodes.end() );
        CHECK_STATE( it->second );
        return it->first;
    }

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

    static uint64_t getBootstrapBlockIDForNode( const ConsensusEngine& e, node_id node ) {
        auto it = e.nodes.find( node );
        CHECK_STATE2( it != e.nodes.end(), "Node with id " + to_string( node ) + " not found" );
        CHECK_STATE( it->second );
        auto schain = it->second->getSchain();
        CHECK_STATE( schain );
        return (uint64_t) schain->getBootstrapBlockID();
    }

    static uint64_t getBootstrapBlockID( const ConsensusEngine& e ) {
        return getBootstrapBlockIDForNode( e, getFirstNodeId( e ) );
    }

    static block_id getCurrentLastCommittedBlockIDForNode( const ConsensusEngine& e, node_id node ) {
        auto it = e.nodes.find( node );
        CHECK_STATE2( it != e.nodes.end(), "Node with id " + to_string( node ) + " not found" );
        CHECK_STATE( it->second );
        auto schain = it->second->getSchain();
        CHECK_STATE( schain );
        return schain->getLastCommittedBlockID();
    }

    static bool haveAllCommitted(
        const ConsensusEngine& e, std::initializer_list< node_id > nodes, block_id blockId ) {
        for ( auto nodeId : nodes ) {
            if ( getCurrentLastCommittedBlockIDForNode( e, nodeId ) < blockId )
                return false;
        }
        return true;
    }

    static block_id getCurrentLastCommittedBlockID( const ConsensusEngine& e ) {
        return getCurrentLastCommittedBlockIDForNode( e, getFirstNodeId( e ) );
    }

    static TimeStamp getCurrentLastCommittedBlockTimeStampForNode(
        const ConsensusEngine& e, node_id node ) {
        auto it = e.nodes.find( node );
        CHECK_STATE2( it != e.nodes.end(), "Node with id " + to_string( node ) + " not found" );
        CHECK_STATE( it->second );
        auto schain = it->second->getSchain();
        CHECK_STATE( schain );
        return schain->getLastCommittedBlockTimeStamp();
    }

    static TimeStamp getCurrentLastCommittedBlockTimeStamp( const ConsensusEngine& e ) {
        return getCurrentLastCommittedBlockTimeStampForNode( e, getFirstNodeId( e ) );
    }

    static ptr< Node > getNode( const ConsensusEngine& e, node_id node ) {
        auto it = e.nodes.find( node );
        CHECK_STATE2( it != e.nodes.end(), "Node with id " + to_string( node ) + " not found" );
        CHECK_STATE( it->second );
        return it->second;
    }

    static void setDbDir( ConsensusEngine& e, const std::string& dir ) {
        e.dbDir = dir;
    }

    // Sends through the real proposal exchange to one peer, bypassing only the sender's
    // turn check. Blocks the caller while the receiver answers CONNECTION_RETRY_LATER.
    static void sendProposalToPeer(
        const ConsensusEngine& e,
        node_id senderNode,
        const ptr< BlockProposal >& proposal,
        schain_index dstIndex ) {
        CHECK_STATE( proposal );
        auto schain = getNode( e, senderNode )->getSchain();
        CHECK_STATE( schain );
        CHECK_STATE( schain->blockProposalClient );
        AbstractClientAgent& client = *schain->blockProposalClient;
        client.sendItem( proposal, dstIndex );
    }

#ifdef BITE
    // Read-only view of a decryptor slot; unlike getMyDecryptionShares(), it does not insert.
    static ptr< AESKeyDecryptionShareList > getStoredDecryptionShareList(
        const ConsensusEngine& e,
        node_id nodeId,
        block_id blockId,
        schain_index decryptorIndex ) {

        auto node = getNode( e, nodeId );
        auto teDb = node->getTEDecryptionDB();
        CHECK_STATE( teDb );
        READ_LOCK( teDb->decryptionSetsMutex );
        const auto blockIt = teDb->decryptionsStore.find( blockId );
        if ( blockIt == teDb->decryptionsStore.end() ) {
            return nullptr;
        }
        const auto shareIt = blockIt->second.find( decryptorIndex );
        if ( shareIt == blockIt->second.end() ) {
            return nullptr;
        }
        return shareIt->second;
    }
#endif
};
