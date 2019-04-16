#pragma once

#include "DataStructure.h"



class PartialHashesList;
class Schain;
class BlockProposal;
class SHAHash;

class BlockProposalSet : public DataStructure  {
    recursive_mutex proposalsMutex;

    //block_id blockID;

    class Comparator {
    public:
        bool operator()(const ptr<SHAHash> &a, const ptr<SHAHash> &b) const {
            if (a->compare(b) < 0)
                return true;
            return false;
        };
    };


    Schain* sChain;

    map< schain_index, ptr< BlockProposal > > proposals;

public:
    node_count getTotalProposalsCount();

    BlockProposalSet( Schain* subChain, block_id blockId );

    bool addProposal(ptr<BlockProposal> _proposal);


    bool isTwoThird();

    ptr< vector< bool > > createBooleanVector();

    ptr< BlockProposal > getProposalByIndex( schain_index _index );



    static uint64_t getTotalObjects() {
        return totalObjects;
    }

    virtual ~BlockProposalSet();

private:

    static atomic<uint64_t>  totalObjects;
};
