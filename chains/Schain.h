

/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file Schain.h
    @author Stan Kladko
    @date 2018
*/

#pragma once


#include "Agent.h"
#include <jsonrpccpp/server/connectors/httpserver.h>
#include "statusserver/StatusServer.h"

class ThresholdSignature;
class CommittedBlockList;
class NetworkMessageEnvelope;
class WorkerThreadPool;
class NodeInfo;
class BlockProposalDB;
class DASigShareDB;
class ServerConnection;
class BlockProposal;
class PartialHashesList;
class DAProof;

class BlockProposalClientAgent;
class BlockProposalPusherThreadPool;

class BlockFinalizeDownloader;
class BlockFinalizeDownloaderThreadPool;

class SchainMessageThreadPool;

class TestMessageGeneratorAgent;
class ConsensusExtFace;

class CatchupClientAgent;
class CatchupServerAgent;
class MonitoringAgent;
class TimeoutAgent;

class BlockProposalServerAgent;

class MessageEnvelope;

class Node;
class PendingTransactionsAgent;

class BlockConsensusAgent;
class PricingAgent;
class IO;
class Sockets;


class BLAKE3Hash;
class ConsensusBLSSigShare;
class ThresholdSigShare;
class BooleanProposalVector;
class TimeStamp;
class CryptoManager;
class StatusServer;

class Schain : public Agent {

    queue< ptr< MessageEnvelope > > messageQueue;

    bool bootStrapped = false;
    bool startingFromCorruptState = false;

    atomic< uint64_t > totalTransactions;

    ConsensusExtFace* extFace = nullptr;

    schain_id schainID = 0;

    ptr<jsonrpc::HttpServer> httpserver;
    ptr<StatusServer> s;


    ptr< TestMessageGeneratorAgent > testMessageGeneratorAgent;

    uint64_t startTimeMs = 0;

    ptr< BlockProposalServerAgent > blockProposalServerAgent;

    ptr< CatchupServerAgent > catchupServerAgent;

    ptr< MonitoringAgent > monitoringAgent;

    ptr< TimeoutAgent > timeoutAgent;

    ptr< PendingTransactionsAgent > pendingTransactionsAgent;

    ptr< BlockProposalClientAgent > blockProposalClient;

    ptr< CatchupClientAgent > catchupClientAgent;

    ptr< PricingAgent > pricingAgent;

    ptr< SchainMessageThreadPool > consensusMessageThreadPool;

    ptr< IO > io;

    ptr< CryptoManager > cryptoManager;

    weak_ptr< Node > node;

    schain_index schainIndex;

    string blockProposerTest;

    atomic< uint64_t > lastCommittedBlockID = 0;
    atomic< uint64_t > lastCommitTimeMs = 0;
    TimeStamp  lastCommittedBlockTimeStamp;
    recursive_mutex lastCommittedBlockInfoMutex;
    atomic<uint64_t> proposalReceiptTime = 0;


    atomic< uint64_t > bootstrapBlockID = 0;
    uint64_t maxExternalBlockProcessingTime = 0;

    uint64_t blockSizeAverage = 0;

    unordered_map <uint64_t, uint64_t> deadNodes;
    recursive_mutex deadNodesLock;

    static ptr<ofstream> visualizationDataStream;
    static recursive_mutex vdsMutex;

public:

    static ptr< ofstream > getVisualizationDataStream();

    void addDeadNode(uint64_t _schainIndex, uint64_t timeMs);

    uint64_t getDeathTime(uint64_t  _schainIndex);

    void markAliveNode(uint64_t  _schainIndex);

    uint64_t getBlockSizeAverage() const;
    uint64_t getBlockTimeAverageMs() const;
    uint64_t getTpsAverage() const;
private:
    uint64_t blockTimeAverageMs = 0 ;
    uint64_t tpsAverage = 0 ;

    ptr< NodeInfo > thisNodeInfo = nullptr;

    void checkForExit();

    void proposeNextBlock();

    void processCommittedBlock( const ptr< CommittedBlock >& _block );

    void startConsensus(
        const block_id _blockID, const ptr< BooleanProposalVector >& _propposalVector );

    void constructChildAgents();

    void saveBlock( const ptr< CommittedBlock >& _block );

    void pushBlockToExtFace( const ptr< CommittedBlock >& _block );

    ptr< BlockProposal > createDefaultEmptyBlockProposal( block_id _blockId );

public:

    bool isStartingFromCorruptState() const;

    void updateLastCommittedBlockInfo( uint64_t _lastCommittedBlockID,
        TimeStamp& _lastCommittedBlockTimeStamp, uint64_t _blockSize );

    void initLastCommittedBlockInfo( uint64_t _lastCommittedBlockID,
                                       TimeStamp&  _lastCommittedBlockTimeStamp );


    uint64_t getLastCommitTimeMs();

    ptr< BlockConsensusAgent > blockConsensusInstance;

    void createBlockConsensusInstance();

    void joinMonitorAndTimeoutThreads();

    ptr< BlockProposal > getBlockProposal( block_id _blockID, schain_index _schainIndex );

    void constructServers( const ptr< Sockets >& _sockets );

    void healthCheck();

    ConsensusExtFace* getExtFace() const;

    uint64_t getMaxExternalBlockProcessingTime() const;

    Schain( weak_ptr< Node > _node, schain_index _schainIndex, const schain_id& _schainID,
        ConsensusExtFace* _extFace );

    Schain();  // empty constructor is used for tests

    void startThreads();

    static void messageThreadProcessingLoop( Schain* _sChain );

    TimeStamp  getLastCommittedBlockTimeStamp();

    void setBlockProposerTest( const string& _blockProposerTest );

    uint64_t getStartTimeMs() const;

    void proposedBlockArrived( const ptr< BlockProposal >& _proposal );

    void daProofArrived( const ptr< DAProof >& _daProof );

    void blockProposalReceiptTimeoutArrived( block_id _blockID );

    void blockCommitArrived( block_id _committedBlockID, schain_index _proposerIndex,
        const ptr< ThresholdSignature >& _thresholdSig );


    void blockCommitsArrivedThroughCatchup( const ptr< CommittedBlockList >& _blockList );

    void daProofSigShareArrived(
        const ptr< ThresholdSigShare >& _sigShare, const ptr< BlockProposal >& _proposal );

    const ptr< IO > getIo() const;

    void postMessage( const ptr< MessageEnvelope >& _me );

    ptr< PendingTransactionsAgent > getPendingTransactionsAgent() const;

    ptr< MonitoringAgent > getMonitoringAgent() const;

    schain_index getSchainIndex() const;

    ptr< Node > getNode() const;

    transaction_count getMessagesCount();

    node_id getNodeIDByIndex( schain_index _index );

    schain_id getSchainID();

    ptr< BlockConsensusAgent > getBlockConsensusInstance();

    ptr< NodeInfo > getThisNodeInfo() const;

    node_count getNodeCount();

    block_id getLastCommittedBlockID() const;

    ptr< CommittedBlock > getBlock( block_id _blockID );

    string getBlockProposerTest() const;

    void setBlockProposerTest( const char* _blockProposerTest );

    ptr< TestMessageGeneratorAgent > getTestMessageGeneratorAgent() const;

    void bootstrap( block_id _lastCommittedBlockID, uint64_t _lastCommittedBlockTimeStamp,
        uint64_t _lastCommittedBlockTimeStampMs );

    uint64_t getTotalTransactions() const;

    block_id getBootstrapBlockID() const;

    void setHealthCheckFile( uint64_t status );

    uint64_t getTotalSigners();

    uint64_t getRequiredSigners();

    u256 getPriceForBlockId( uint64_t _blockId );

    ptr< CryptoManager > getCryptoManager() const;

    void finalizeDecidedAndSignedBlock( block_id _blockId, schain_index _proposerIndex,
        const ptr< ThresholdSignature >& _thresholdSig );

    void tryStartingConsensus( const ptr< BooleanProposalVector >& pv, const block_id& bid );

    bool fixCorruptStateIfNeeded( block_id id );
    void rebroadcastAllMessagesForCurrentBlock() const;

    static void bumpPriority();
    static void unbumpPriority();
    void startStatusServer();
    void stopStatusServer();
    void setLastCommittedBlockId( uint64_t lastCommittedBlockId );
};

