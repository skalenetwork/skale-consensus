//
// Created by kladko on 19.09.19.
//



#include <boost/integer/integer_log2.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>


#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/SerializeException.h"

#include "CommittedBlockFragment.h"



#include "CommittedBlockFragmentList.h"

CommittedBlockFragmentList::CommittedBlockFragmentList(const block_id &_blockId,
                                                       const uint64_t _totalFragments) :
                                                                                        blockID(_blockId),
                                                                                        totalFragments(
                                                                                                _totalFragments) {
    CHECK_ARGUMENT(totalFragments > 0);

    for (uint64_t i = 1; i <= totalFragments; i++) {
        missingFragments.push_back(i);
    }

}

bool CommittedBlockFragmentList::addFragment(ptr<CommittedBlockFragment> _fragment, uint64_t& nextIndexToRetrieve) {

    CHECK_ARGUMENT(_fragment->getBlockId() == blockID);
    CHECK_ARGUMENT(_fragment->getIndex() > 0)
    CHECK_ARGUMENT(_fragment->getIndex() <= totalFragments);
    CHECK_ARGUMENT(_fragment->getData() != nullptr)

    lock_guard<recursive_mutex> lock(listMutex);

    checkSanity();


    nextIndexToRetrieve = 0;

    if (fragments.find(_fragment->getIndex()) != fragments.end()) {
        return false;
    }


    fragments[_fragment->getIndex()] = _fragment->getData();


    std::list<uint64_t >::iterator findIter = std::find(missingFragments.begin(), missingFragments.end(), _fragment->getIndex());

    ASSERT(findIter != missingFragments.end());

    missingFragments.erase(findIter);

    if (isComplete()) {
        return true;
    }



    ASSERT(missingFragments.size() > 0);

    uint64_t randomPosition = ubyte(gen) % missingFragments.size();

    uint64_t  j = 0;

    for (auto && element: missingFragments) {
        if (j == randomPosition) {
            nextIndexToRetrieve = element;
        }
        j++;
    }


    ASSERT(nextIndexToRetrieve > 0);

    return true;
}

void CommittedBlockFragmentList::checkSanity() {
    CHECK_STATE(fragments.size() <= totalFragments);
}

bool CommittedBlockFragmentList::isComplete() {
    lock_guard<recursive_mutex> lock(listMutex);

    checkSanity();

    if (fragments.size() == totalFragments) {
        for (uint64_t i = 1; i <= totalFragments; i++) {
            CHECK_STATE(fragments.find(i) != fragments.end())
        }


        CHECK_STATE(missingFragments.size() == 0);

        return true;
    }

    return false;
}

ptr<vector<uint8_t>> CommittedBlockFragmentList::serialize() {


    auto result = make_shared<vector<uint8_t >>();

    lock_guard<recursive_mutex> lock(listMutex);

    CHECK_STATE(isComplete());

    CHECK_STATE(!isSerialized)

    isSerialized = true;

    uint64_t totalLen = 0;

    try {

        for (auto &&item : fragments) {
            totalLen += item.second->size() - 2;
        }

        result->reserve(totalLen);

        for (auto &&item : fragments) {
            auto fragment = item.second;
            result->insert(result->end(), fragment->begin() + 1, fragment->end() - 1);
        }

    } catch (...) {
        throw_with_nested(SerializeException("Could not serialize fragments", __CLASS_NAME__));
    }


    CHECK_STATE(result->size() == totalLen);

    CHECK_STATE((*result)[sizeof(uint64_t)] == '{');
    CHECK_STATE(result->back() == '>');
    return result;
}



boost::random::mt19937 CommittedBlockFragmentList::gen;

boost::random::uniform_int_distribution<> CommittedBlockFragmentList::ubyte(0, 1024);