//
// Created by kladko on 19.09.19.
//
#include "../SkaleCommon.h"
#include "../Log.h"
#include "CommittedBlockFragment.h"
#include "CommittedBlockFragmentList.h"

CommittedBlockFragmentList::CommittedBlockFragmentList(const block_id &_blockId,
                                                       const uint64_t _totalFragments) :
                                                                                        blockID(_blockId),
                                                                                        totalFragments(
                                                                                                _totalFragments) {}

bool CommittedBlockFragmentList::addFragment(ptr<CommittedBlockFragment> _fragment) {

    CHECK_ARGUMENT(_fragment->getBlockId() == blockID);
    CHECK_ARGUMENT(_fragment->getIndex() > 0)
    CHECK_ARGUMENT(_fragment->getIndex() <= totalFragments);
    CHECK_ARGUMENT(_fragment->getData() != nullptr)

    lock_guard<recursive_mutex> lock(listMutex);

    checkSanity();

    if (fragments.find(_fragment->getIndex()) != fragments.end()) {
        return false;
    }


    fragments[_fragment->getIndex()] = _fragment->getData();

    return true;
}

void CommittedBlockFragmentList::checkSanity() {
    CHECK_STATE(fragments.size() <= totalFragments);
}

bool CommittedBlockFragmentList::isComplete() {
    lock_guard<recursive_mutex> lock(listMutex);

    checkSanity();

    return (fragments.size() == totalFragments);

}

ptr<vector<uint8_t>> CommittedBlockFragmentList::serialize() {
    lock_guard<recursive_mutex> lock(listMutex);

    CHECK_STATE(isComplete());

    CHECK_STATE(!isSerialized)

    isSerialized = true;

    uint64_t totalLen = 0;

    for (uint64_t i = 0; i <= totalFragments; i++) {
        totalLen += fragments.at(i)->size();
    }

    auto result = make_shared<vector<uint8_t>>();
    result->reserve(totalLen);

    for (uint64_t i = 0; i <= totalFragments; i++) {

        auto fragment = fragments.at(i);
        result->insert(result->end(), fragment->begin() + 1, fragment->end() - 1);
    }


    CHECK_STATE(result->size() == totalLen);

    return result;
}
