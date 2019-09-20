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

    @file CommittedBlock.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#define BOOST_PENDING_INTEGER_LOG2_HPP
#include <boost/integer/integer_log2.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "CommittedBlockFragmentList.h"

#include "BlockProposal.h"

class Schain;

class CommittedBlockFragment;

class CommittedBlock : public BlockProposal {

    ptr< vector< uint8_t > > serializedBlock = nullptr;

    CommittedBlock( uint64_t timeStamp, uint32_t timeStampMs );

    ptr< vector< uint64_t > > parseBlockHeader( const shared_ptr< string >& header );

public:
    CommittedBlock( Schain& _sChain, ptr< BlockProposal > _p );
    CommittedBlock( const schain_id& sChainId, const node_id& proposerNodeId,
        const block_id& blockId, const schain_index& proposerIndex,
        const ptr< TransactionList >& transactions, uint64_t timeStamp, __uint32_t timeStampMs );

    ptr<CommittedBlockFragment> getFragment(uint64_t _totalFragments, fragment_index _index);



    static ptr< CommittedBlock > deserialize( ptr< vector< uint8_t > > _serializedBlock );

    static ptr< CommittedBlock > defragment( ptr<CommittedBlockFragmentList> _fragmentList );

    ptr< vector< uint8_t > > getSerialized();


    static ptr< CommittedBlock > createRandomSample( uint64_t _size, boost::random::mt19937& _gen,
        boost::random::uniform_int_distribution<>& _ubyte, block_id _blockID = block_id( 1 ) );



};
