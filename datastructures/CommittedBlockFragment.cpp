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

    @file CommittedBlockFragment.cpp
    @author Stan Kladko
    @date 2019
*/
#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/ParsingException.h"

#include "CommittedBlockFragment.h"

CommittedBlockFragment::CommittedBlockFragment(const block_id &blockId, const uint64_t totalFragments,
                                               const fragment_index &fragmentIndex, const ptr<vector<uint8_t>> &data,
                                               uint64_t _blockSize, ptr<string> _blockHash) :
        blockId(blockId), totalFragments(totalFragments),
        fragmentIndex(fragmentIndex), data(data), blockSize(_blockSize), blockHash(_blockHash) {
    CHECK_ARGUMENT(totalFragments > 0);
    CHECK_ARGUMENT(fragmentIndex <= totalFragments);
    CHECK_ARGUMENT(data != nullptr);
    CHECK_ARGUMENT(blockId > 0);
    CHECK_ARGUMENT(data->size() > 0);
    if (data->size() < 3) {
        BOOST_THROW_EXCEPTION(ParsingException("Data fragment too short:" +
         to_string(data->size()), __CLASS_NAME__));
    }

    if(data->front() != '<') {
        BOOST_THROW_EXCEPTION(ParsingException("Data fragment does not start with <", __CLASS_NAME__));
    }

    if(data->back() != '>') {
        BOOST_THROW_EXCEPTION(ParsingException("Data fragment does not end with >", __CLASS_NAME__));
    }
}

uint64_t CommittedBlockFragment::getBlockSize() const {
    return blockSize;
}

ptr<string> CommittedBlockFragment::getBlockHash() const {
    return blockHash;
}


block_id CommittedBlockFragment::getBlockId() const {
    return blockId;
}

uint64_t CommittedBlockFragment::getTotalFragments() const {
    return totalFragments;
}

fragment_index CommittedBlockFragment::getIndex() const {
    return fragmentIndex;
}

ptr<vector<uint8_t>> CommittedBlockFragment::serialize() const {
    return data;
}