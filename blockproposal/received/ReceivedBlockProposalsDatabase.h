#pragma once


class BlockProposalSet;

class PartialHashesList;

class Schain;


class ReceivedBlockProposalsDatabase : Agent {


    recursive_mutex proposalsDatabaseMutex;

    block_id oldBlockID;

    map<block_id, ptr<BlockProposalSet>> proposedBlockSets;





public:



    ptr<BlockProposalSet> getProposedBlockSet(block_id _blockID);


    ptr<BlockProposal> getBlockProposal(block_id blockID, schain_index proposerIndex);


    ReceivedBlockProposalsDatabase(Schain &_sChain);

    void cleanOldBlockProposals(block_id _lastCommittedBlockID);


    bool addBlockProposal(ptr<BlockProposal> _proposal);


    ptr<vector<bool>> getBooleanProposalsVector(block_id _blockID);

    bool isTwoThird(block_id _blockID);
};



