//
// Created by kladko on 19.09.19.
//
#include "../SkaleCommon.h"
#include "../Log.h"
#include "CommittedBlockFragment.h"
#include "CommittedBlockFragmentList.h"
#include "../exceptions/SerializeException.h"

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


    auto result = make_shared < vector < uint8_t >> ();

    lock_guard<recursive_mutex> lock(listMutex);

    CHECK_STATE(isComplete());

    CHECK_STATE(!isSerialized)

    isSerialized = true;

    uint64_t totalLen = 0;

    try {

        for (uint64_t i = 1; i <= totalFragments; i++) {
            totalLen += fragments.at(i)->size();
        }

        result->reserve(totalLen);

        for (uint64_t i = 0; i <= totalFragments; i++) {

            auto fragment = fragments.at(i);
            result->insert(result->end(), fragment->begin() + 1, fragment->end() - 1);
        }

    } catch(...) {
        throw_with_nested(SerializeException("Could not serialize fragments", __CLASS_NAME__));
    }


    CHECK_STATE(result->size() == totalLen);

    return result;
}
