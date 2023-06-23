/*
    Copyright (C) 2019 SKALE Labs

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

    @file BlockProposalFragmentList.h
    @author Stan Kladko
    @date 2019
*/


#ifndef SKALED_BLOCKPROPOSALFRAGMENTLIST_H
#define SKALED_BLOCKPROPOSALFRAGMENTLIST_H


class BlockProposalFragment;


#include <boost/integer/integer_log2.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "DataStructure.h"


class BlockProposalFragmentList : public DataStructure {
    map< fragment_index, ptr< vector< uint8_t > > > fragments;  // tsafe

    list< uint64_t > missingFragments;

    int64_t blockSize = -1;

    string blockHash;

    bool isSerialized = false;

    const block_id blockID = 0;

    const uint64_t totalFragments = 0;

    void checkSanity();

    static boost::random::mt19937 gen;

    static boost::random::uniform_int_distribution<> ubyte;

public:
    BlockProposalFragmentList( const block_id& _blockId, uint64_t _totalFragments );

    bool addFragment(
        const ptr< BlockProposalFragment >& _fragment, uint64_t& _nextIndexToRetrieve );

    uint64_t nextIndexToRetrieve();

    bool isComplete();

    const ptr< vector< uint8_t > > serialize();
};


#endif  // SKALED_BLOCKPROPOSALFRAGMENTLIST_H
