//
// Created by stan on 15-03-2025.
//

#include "SkaleCommon.h"
#include "Log.h"
#include "chains/Schain.h"
#include "FlatBufferRequest.h"

#include "BlockHeaderObject.h"

using namespace  block_finalize;

void FlatBufferRequest::verify(schain_id _sChainId) noexcept( false ) {
    if (schainId != _sChainId) {
        throw std::invalid_argument("Invalid schain:" + to_string(schainId) + "!="
                                    + to_string(_sChainId));
    }
}

shared_ptr<std::vector<uint8_t> > FlatBufferRequest::copyFbByteVector(
    const flatbuffers::Vector<uint8_t> *_fbVector) {
    if (!_fbVector) {
        return nullptr;
    } else {
        return make_shared<vector<uint8_t> >(_fbVector->begin(), _fbVector->end());
    }
}

shared_ptr<std::vector<transaction_index> > FlatBufferRequest::copyFbIndexVector(
    const flatbuffers::Vector<uint16_t> *_fbVector) {
    if (!_fbVector) {
        return nullptr;
    } else {
        return make_shared<vector<transaction_index> >(_fbVector->begin(), _fbVector->end());
    }
}


template<typename T, typename U>
void FlatBufferRequest::copyFbArray(const T *_fbData, U &_dest) {
    CHECK_STATE(_fbData);
    std::copy(_fbData->begin(), _fbData->end(), _dest.begin());
}

// Function to copy hashes from source to destination
template <typename T, typename U>
void FlatBufferRequest::copyFbHashList(const shared_ptr<vector<ptr<T>>>& _dest, const U& _src, size_t _expectedSize) {
    CHECK_STATE(_src);
    _dest->reserve(_src->size());
    for (const auto& fbHash : *_src) {
        CHECK_STATE(fbHash && fbHash->data() && fbHash->data()->size() == _expectedSize);
        auto hash = make_shared<T>();
        std::copy(fbHash->data()->begin(), fbHash->data()->end(), hash->begin());
        _dest->push_back(hash);
    }
}




