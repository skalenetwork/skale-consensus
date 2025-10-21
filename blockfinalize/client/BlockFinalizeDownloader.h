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

#include <folly/synchronization/Baton.h>
#include <folly/SharedMutex.h>
#include "datastructures/BlockProposalFragmentList.h"

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

    std::atomic<uint64_t> fragmentDownloadCounter = 0;

#ifdef BITE
    // we already have the proposal, all we need is threshold decryptions
    bool needFragmentData;
#endif

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


    ptr< BlockProposalFragment > readBlockFragment( const ptr< ClientSocket >& _socket,
        nlohmann::json responseHeader, fragment_index _fragmentIndex, node_count _nodeCount
#ifdef BITE
        , schain_index _proposerIndex
        , schain_index _destinationIndex
#endif
    );

    static uint64_t readFragmentSize( nlohmann::json _responseHeader );

    bool downloadProposalDAProofAndDecryptions();


    bool completeAndNeedToExitAllThreads();

    string readBlockHash( nlohmann::json _responseHeader );

    block_id getBlockId();

    schain_index getProposerIndex();

    static uint64_t readBlockSize( nlohmann::json _responseHeader );

    string readDAProofSig( nlohmann::json _responseHeader );

    void processDAProofSig(nlohmann::json _responseHeader, string h);


    bool needDAProof();
#ifdef BITE
    bool needDecryptionShares(schain_index _decryptorIndex);
#endif

    bool exitDownloadLoop(uint64_t _nextFragmentToDownload);

    void waitAfterNetworkError();

    void waitAfterNoProposal();

    uint64_t nextFragmentToDownload();

    static uint64_t computeFirstFragmentToDowload(schain_index _dstIndex, schain_index _mySchainIndex);
};
