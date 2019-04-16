#pragma once



#include "../ProtocolKey.h"

#include "../ProtocolInstance.h"



class ChildBVDecidedMessage;
class BlockProposalSet;
class Schain;


class BlockConsensusAgent {

    Schain &  sChain;


    class Comparator {
    public:
        bool operator()(const ptr<ProtocolKey> &a,
                        const ptr<ProtocolKey>& b ) const {

            return *a < *b;
            return *a < *b;

        }
    };



    bin_consensus_round cleanedRound = bin_consensus_round(0);


    recursive_mutex childrenMutex;

    map<ptr<ProtocolKey>, ptr<BinConsensusInstance>, Comparator> children;




    map<ptr<ProtocolKey>, ProtocolOutcome , Comparator> completedInstancesByProtocolKey;


    set<block_id> proposedBlocks;

    map<block_id , set<schain_index>> trueDecisions;

    map<block_id , set<schain_index>> falseDecisions;


    map<block_id, schain_index>  decidedBlocks;

    void processChildMessageImpl(ptr<InternalMessageEnvelope> _me);

    void decideBlock(block_id _blockNumber, schain_index subChainIndex);


    void propose(bin_consensus_value _proposal, schain_index index, block_id _id);

    void voteAndDecideIfNeded(ptr<ChildBVDecidedMessage> msg);

    void voteAndDecideIfNeded1(ptr<ChildBVDecidedMessage> msg);

    void decideEmptyBlock(block_id blockNumber);

    void disconnect(ptr<ProtocolKey> key );

    void processChildCompletedMessage(ptr<InternalMessageEnvelope> _me);

    void startConsensusProposal(block_id _blockID, ptr<vector<bool>> _proposal);


    void processMessage(ptr<MessageEnvelope> _m);


public:


    BlockConsensusAgent(Schain& _schain);


    bin_consensus_round getRound(ptr<ProtocolKey> _key);

    bool decided(ptr<ProtocolKey> key);


    Schain *getSchain() const;


    void routeAndProcessMessage(ptr<MessageEnvelope> m);

    ptr<BinConsensusInstance> getChild(ptr<ProtocolKey> key);


};

