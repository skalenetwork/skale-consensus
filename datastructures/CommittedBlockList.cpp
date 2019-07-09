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

    @file CommittedBlockList.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/NetworkProtocolException.h"
#include "../crypto/SHAHash.h"
#include "CommittedBlock.h"

#include "CommittedBlockList.h"


CommittedBlockList::CommittedBlockList(ptr<vector<ptr<CommittedBlock>>> _blocks) {

    ASSERT(_blocks);
    ASSERT(_blocks->size() > 0);

    blocks = _blocks;

}


CommittedBlockList::CommittedBlockList(ptr<vector<size_t>> _blockSizes, ptr<vector<uint8_t>> _serializedBlocks) {

    size_t index = 0;
    uint64_t counter = 0;

    blocks = make_shared<vector<ptr<CommittedBlock>>>();

    for (auto &&size : *_blockSizes) {

        auto endIndex = index + size;

        ASSERT(endIndex <= _serializedBlocks->size());

        auto blockData = make_shared<vector<uint8_t>>(
                _serializedBlocks->begin() + index,
                _serializedBlocks->begin() + endIndex);

        auto block = CommittedBlock::deserialize(blockData);

        blocks->push_back(block);

        index = endIndex;

        counter++;
    }


};


ptr<vector<ptr<CommittedBlock>>> CommittedBlockList::getBlocks() {
    return blocks;
}

shared_ptr<vector<uint8_t>>CommittedBlockList::serialize()  {

    size_t totalSize = 0;

    for (auto &&block : *blocks) {
        totalSize += block->serialize()->size();
    }


    auto serializedBlocks = make_shared<vector<uint8_t>>();

    for (auto && block : *blocks) {
        auto data = block->serialize();
        serializedBlocks->insert(serializedBlocks->end(), data->begin(), data->end());
    }
    return serializedBlocks;
}
