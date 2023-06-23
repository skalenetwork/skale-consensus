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

    @file CommittedBlockList.h
    @author Stan Kladko
    @date 2018 -
*/


#pragma once


#define BOOST_PENDING_INTEGER_LOG2_HPP

#include <boost/integer/integer_log2.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "DataStructure.h"


class CommittedBlock;

class CommittedBlockList : public DataStructure {
    ptr< vector< ptr< CommittedBlock > > > blocks;  // tsafe

    CommittedBlockList( const ptr< CryptoManager >& _cryptoManager,
        const ptr< vector< uint64_t > >& _blockSizes,
        const ptr< vector< uint8_t > >& _serializedBlocks, uint64_t offset = 0,
        bool _createPartialListIfSomeSignaturesDontVerify = false );

public:
    explicit CommittedBlockList( const ptr< vector< ptr< CommittedBlock > > >& _blocks );


    ptr< vector< ptr< CommittedBlock > > > getBlocks();

    ptr< vector< uint64_t > > createSizes();

    ptr< vector< uint8_t > > serialize();

    static ptr< CommittedBlockList > deserialize( const ptr< CryptoManager >& _cryptoManager,
        const ptr< vector< uint64_t > >& _blockSizes,
        const ptr< vector< uint8_t > >& _serializedBlocks, uint64_t _offset,
        bool _createPartialListIfSomeSignaturesDontVerify = false );


    static ptr< CommittedBlockList > createRandomSample( const ptr< CryptoManager >& _cryptoManager,
        uint64_t _size, boost::random::mt19937& _gen,
        boost::random::uniform_int_distribution<>& _ubyte );
};
