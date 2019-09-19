//
// Created by kladko on 19.09.19.
//

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/ParsingException.h"

#include "CommittedBlockFragment.h"

CommittedBlockFragment::CommittedBlockFragment(const block_id &_blockId, const uint64_t _totalFragments,
                                               const fragment_index &_fragmentIndex, const ptr<vector<uint8_t>>& _data) :
                                               blockId(_blockId), totalFragments(_totalFragments),
                                               fragmentIndex(_fragmentIndex), data(_data) {
    CHECK_ARGUMENT(_totalFragments > 0);
    CHECK_ARGUMENT(_fragmentIndex < _totalFragments);
    CHECK_ARGUMENT(data != nullptr);
    CHECK_ARGUMENT(_blockId > 0);
    CHECK_ARGUMENT(_data->size() > 0);
    if (_data->size() < 3 || _data->front() != '<' || _data->back() != '>') {
        BOOST_THROW_EXCEPTION(ParsingException("Invalid fragment", __CLASS_NAME__));
    }
    CHECK_ARGUMENT(_data->back() == '>');
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

ptr<vector<uint8_t>> CommittedBlockFragment::getData() const {
    return data;
}