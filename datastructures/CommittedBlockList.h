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

    @file CommittedBlockList.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#include "DataStructure.h"




class CommittedBlock;

class CommittedBlockList : public DataStructure {


    ptr<vector<ptr<CommittedBlock>>> blocks = nullptr;

public:


    CommittedBlockList(ptr<vector<size_t>> _blockSizes, ptr<vector<uint8_t>> _serializedBlocks);

    CommittedBlockList(ptr<vector<ptr<CommittedBlock>>> _blocks);

    ptr<vector<ptr<CommittedBlock>>> getBlocks() ;

    shared_ptr<vector<uint8_t>> serialize() ;

};



