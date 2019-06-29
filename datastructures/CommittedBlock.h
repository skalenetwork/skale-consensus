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

#include "BlockProposal.h"

class Schain;

class CommittedBlock : public  BlockProposal {


    uint64_t  headerSize = 0;
public:
    uint64_t getHeaderSize() const;

public:

    CommittedBlock(Schain& _sChain, ptr<BlockProposal> _p);

    CommittedBlock(ptr<vector<uint8_t>> _serializedBlock);

    ptr<vector<uint8_t>> serialize();

    ptr<vector<uint8_t>> serializedBlock = nullptr;

    ptr<std::vector<unsigned long, std::allocator<unsigned long>>> parseBlockHeader(const shared_ptr<string> &header);
};