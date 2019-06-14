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

    @file BlockProposalSet.h
    @author Stan Kladko
    @date 2018
*/

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

    map< schain_index, ptr< BlockProposal > > proposals; // converted

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
