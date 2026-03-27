/*
    Copyright (C) 2019- SKALE Labs

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

    @file BlockFinalizeDownloader.h
    @author Stan Kladko
    @date 2019
*/


#pragma once


class CommittedBlockList;
class ClientSocket;
class Schain;
class BlockFinalizeResponseHeader;
class BlockProposalFragment;
class BlockProposalFragmentList;
class BlockProposal;
class BlockFinalizeDownloaderThreadPool;
class BlockProposalSet;
class ThresholdSignature;
class Header;

#include <folly/synchronization/Baton.h>
#include <folly/SharedMutex.h>
#include "datastructures/BlockProposalFragmentList.h"

namespace {

struct BlockFinalizeResponse {
    nlohmann::json header;
    ptr< vector< uint8_t > > payload;
};

}  // namespace


/**
 * Client-side BlockFinalize recovery agent.
 *
 * Schain creates this after a block has already been decided and signed, but the local node still
 * lacks some data required to finalize it locally. The downloader contacts peer nodes and fetches:
 * - missing proposal fragments
 * - the DA proof signature, when needed
 * - under BITE, missing decryption shares from other nodes
 *
 * Transport behavior:
 * - prefer the new persistent ZMQ bulk-data lane
 * - fall back to the legacy per-request TCP CATCHUP path for older or unavailable peers
 * - cache that fallback decision per peer for a short cooldown period
 */
class BlockFinalizeDownloader : public Agent {
    block_id blockId = 0;

#ifdef  BITE
    epoch_id epochId = 0;
#endif

    schain_index proposerIndex = 0;

    BlockProposalFragmentList fragmentList;

private:
    string blockHash = "";
    ptr< ThresholdSignature > daSig = nullptr;

    recursive_mutex m;

    // Track which nodes are using old TCP interface.
    // We keep a timestamp until which we should keep using TCP (even if ZMQ is available) 
    // for each node that we know is using TCP fallback, to avoid repeatedly trying ZMQ for nodes that don't support it.
    // Eventually all nodes will move to ZMQ, and this map will become mostly unused.
    map< schain_index, uint64_t > tcpFallbackUntilMs;

    std::atomic<uint64_t> fragmentDownloadCounter = 0;

#ifdef BITE
    // we already have the proposal, all we need is threshold decryptions
    bool needFragmentData;
#endif

    bool isFragmentDownloadComplete();

public:

    // this is used to signal to the outside world that
    // downloader completed the download and consensus has everything
    // to commit the block
    atomic<bool> downloadCompleted = false;
    folly::Baton<> downLoadCompletedBaton;

    ptr< ThresholdSignature > getDaSig( uint64_t _blockTimeStampS );

    ptr< BlockFinalizeDownloaderThreadPool > threadPool = nullptr;

    BlockFinalizeDownloader( Schain* _sChain, block_id _blockId,
#ifdef  BITE
    epoch_id _epochId,
#endif
    schain_index _proposerIndex);

    ~BlockFinalizeDownloader() override;

    void downloadFragment( schain_index _dstIndex, fragment_index _fragmentIndex );


    static void workerThreadFragmentDownloadLoop(
        BlockFinalizeDownloader* _agent, schain_index _dstIndex );

    void joinAllThreads();

    nlohmann::json readBlockFinalizeResponseHeader( const ptr< ClientSocket >& _socket );

    /**
     * Downloads block fragment from '_dstIndex' node using TCP connection. 
     * This is the old interface, used only as a fallback when ZMQ download fails, 
     * and is marked in local cache to keep using TCP for this node for some time in the future.
     */
    void downloadFragmentTCP( schain_index _dstIndex, fragment_index _fragmentIndex,
        const ptr< Header >& _header );

    /**
     * Downloads block fragment from '_dstIndex' node using ZMQ connection.
     * This is the new interface, preferred over TCP, and should be used in newer nodes (that 
     * execute current codebase)
     */
    void downloadFragmentZMQ( schain_index _dstIndex, fragment_index _fragmentIndex,
        const ptr< Header >& _header );

    /**
     * Check local cache if '_dstIndex' node was using TCP
     * recently. If so, it should be marked to keep TCP fallback.
     * Else, returns false, meaning we can try ZMQ for this node.
     */
    bool shouldUseTCPFallback( schain_index _dstIndex );

    /**
     * Mark '_dstIndex' node to use TCP fallback for some time in the future,
     * based on current time + configured TCP fallback duration.
     */
    void markTCPFallback( schain_index _dstIndex );

    /**
     * Clears TCP fallback mark for '_dstIndex' node, 
     * allowing to try ZMQ for this node again.
     */
    void clearTCPFallback( schain_index _dstIndex );

    ptr< vector< uint8_t > > readSerializedBlockFragment(
        const ptr< ClientSocket >& _socket, const nlohmann::json& _responseHeader );

    void validateBlockFinalizeResponse(
        const nlohmann::json& _responseHeader, schain_index _dstIndex, fragment_index _fragmentIndex );

    void processBlockFinalizePayload( schain_index _dstIndex, fragment_index _fragmentIndex,
        const nlohmann::json& _responseHeader, const ptr< vector< uint8_t > >& _serializedFragment );


    ptr< BlockProposalFragment > readBlockFragment( const ptr< vector< uint8_t > >& _serializedFragment,
        const nlohmann::json& responseHeader, fragment_index _fragmentIndex, node_count _nodeCount
#ifdef BITE
        , schain_index _proposerIndex
        , schain_index _destinationIndex
#endif
    );

    static uint64_t readFragmentSize( const nlohmann::json& _responseHeader );

    bool downloadProposalDAProofAndDecryptions();


    bool completeAndNeedToExitAllThreads();

    string readBlockHash( const nlohmann::json& _responseHeader );

    block_id getBlockId();

    schain_index getProposerIndex();

    static uint64_t readBlockSize( const nlohmann::json& _responseHeader );

    string readDAProofSig( const nlohmann::json& _responseHeader );

    void processDAProofSig(const nlohmann::json& _responseHeader, string h);


    bool needDAProof();
#ifdef BITE
    /**
     * Checks if we have enough shares from other nodes.
     * If we do, we don't need this node's shares - return false.
     * Else, check if we have enough shares from '_decryptorIndex' node.
     * If not, return true.
     */
    bool needDecryptionShares(schain_index _decryptorIndex);
#endif

    bool exitDownloadLoop(uint64_t _nextFragmentToDownload);

    void waitAfterNetworkError();

    void waitAfterNoProposal();

    uint64_t nextFragmentToDownload();

    static uint64_t computeFirstFragmentToDowload(schain_index _dstIndex, schain_index _mySchainIndex);
};
