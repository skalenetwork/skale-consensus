/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file Schain.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once

#include "../Agent.h"


class CommittedBlockList;
class NetworkMessageEnvelope;
class WorkerThreadPool;
class NodeInfo;
class ReceivedBlockProposalsDatabase;
class ReceivedSigSharesDatabase;
class Connection;
class BlockProposal;
class PartialHashesList;


class BlockProposalClientAgent;
class BlockProposalPusherThreadPool;

class BlockFinalizeClientAgent;

class SchainMessageThreadPool;

class TestMessageGeneratorAgent;
class ConsensusExtFace;
class ExternalQueueSyncAgent;

class CatchupClientAgent;

class CatchupServerAgent;

class MessageEnvelope;

class Node;
class PendingTransactionsAgent;

class BlockConsensusAgent;
class IO;



class SHAHash;
class BLSSigShare;


class Schain : public Agent {


    bool bootStrapped = false;

    atomic<uint64_t>  totalTransactions;


    ConsensusExtFace* extFace = nullptr;


private:


    /**
     * ID of this SChain
     */
    schain_id  schainID;


    ptr<TestMessageGeneratorAgent> testMessageGeneratorAgent;

    ptr<ExternalQueueSyncAgent> externalQueueSyncAgent;

    chrono::milliseconds startTime;

    std::map<block_id, ptr<CommittedBlock>> blocks;

    block_id returnedBlock = 0;


    set<block_id> pushedBlockProposals;

    set<block_id> startedConsensuses;

    recursive_mutex startedConsensusesMutex;


    ptr<PendingTransactionsAgent> pendingTransactionsAgent;

    ptr<BlockProposalClientAgent> blockProposalClient;

    ptr<BlockFinalizeClientAgent> blockFinalizeClient;

    ptr<CatchupClientAgent> catchupClientAgent;

    ptr<CatchupServerAgent> catchupServerAgent;

    ptr<SchainMessageThreadPool> consensusMessageThreadPool = nullptr;


    ptr<BlockConsensusAgent> blockConsensusInstance;

    ptr<IO> io;



    Node& node;


    schain_index schainIndex;

    ptr<string> blockProposerTest ;


    /*** Queue of unprocessed messages for this schain instance
 */
    queue<ptr<MessageEnvelope>> messageQueue;


    /*** Queue of unprocessed messages for this schain instance
 */
    queue<uint64_t> dispatchQueue;




    ptr<NodeInfo> thisNodeInfo = nullptr;

    void proposeNextBlock(uint64_t _previousBlockTimeStamp);


    void processCommittedBlock(ptr<CommittedBlock> _block);



    void startConsensus(block_id _blockID);

    atomic<uint64_t> committedBlockID;


    atomic<uint64_t> bootstrapBlockID;


    atomic<uint64_t>committedBlockTimeStamp;

    void constructChildAgents();



public:

    void healthCheck();

    ConsensusExtFace *getExtFace() const;


    Schain(Node &_node, schain_index _schainIndex, const schain_id &_schainID, ConsensusExtFace *_extFace);

    void startThreads();

    static void messageThreadProcessingLoop(Schain *s);


    uint64_t getCommittedBlockTimeStamp();


    const ptr<ExternalQueueSyncAgent> &getExternalQueueSyncAgent() const;

    void setBlockProposerTest(const string &blockProposerTest);


    ptr<ReceivedBlockProposalsDatabase> blockProposalsDatabase;

    ptr<ReceivedSigSharesDatabase> sigSharesDatabase;

    chrono::milliseconds getStartTime() const;

    void proposedBlockArrived(ptr<BlockProposal> pbm);

    void blockCommitArrived(bool bootstrap, block_id _committedBlockID, schain_index _proposerIndex,
                                uint64_t _committedTimeStamp);


    void blockCommitsArrivedThroughCatchup(ptr<CommittedBlockList> _blocks);

    void sigShareArrived(ptr<BLSSigShare> _sigShare);

    const ptr<IO> &getIo() const;




    void postMessage(ptr<MessageEnvelope> m);




    const ptr<PendingTransactionsAgent> &getPendingTransactionsAgent() const;


    schain_index getSchainIndex() const;

    Node *getNode() const;


    transaction_count getMessagesCount();



    node_id getNodeID(schain_index _index);

    schain_id getSchainID();


    ptr<BlockConsensusAgent> getBlockConsensusInstance();


    const ptr<NodeInfo> &getThisNodeInfo() const;


    node_count getNodeCount();


    const block_id getCommittedBlockID() const;

    ptr<CommittedBlock> getCachedBlock(block_id _blockID);

    ptr<CommittedBlock> getBlock(block_id _blockID);


    const ptr<string> getBlockProposerTest() const {
        return blockProposerTest;
    }

    void setBlockProposerTest(const char *_blockProposerTest) {
        blockProposerTest = make_shared<string>(_blockProposerTest);
    }

    const ptr<TestMessageGeneratorAgent> &getTestMessageGeneratorAgent() const;


    void bootstrap(block_id _lastCommittedBlockID, uint64_t _lastCommittedBlockTimeStamp);

    schain_id getSchainID() const;

    uint64_t getTotalTransactions() const;

    static uint64_t  getHighResolutionTime();

    static chrono::milliseconds getCurrentTimeMilllis();

    static uint64_t getCurrentTimeSec();

    block_id getBootstrapBlockID() const;


    void setHealthCheckFile(uint64_t status);

    void
    pushBlockToExtFace(ptr<CommittedBlock> &_block);

    void saveBlockToLevelDB(ptr<CommittedBlock> &_block);

    void saveBlockToBlockCache(ptr<CommittedBlock> &_block);

    void saveBlock(ptr<CommittedBlock> &_block);



    ptr<vector<uint8_t>> getSerializedBlockFromLevelDB(const block_id &_blockID);



};
