//
// Created by stan on 15-03-2025.
//

#include "SkaleCommon.h"
#include "Log.h"
#include "chains/Schain.h"
#include "FlatBufferRequest.h"

#include "BlockHeaderObject.h"

using namespace block_finalize;

FlatBufferRequest::FlatBufferRequest(schain_id _schainId, epoch_id _epochId, block_id _blockId, node_id _nodeId,
                                     schain_index _proposerIndex)
    : schainId(_schainId),
      epochId(_epochId),
      blockId(_blockId),
      nodeId(_nodeId),
      proposerIndex(_proposerIndex) {
    CHECK_STATE2(schainId == Node::getSchainId(), "Invalid schain:" + to_string(schainId) + "!="
                 + to_string(Node::getSchainId()));
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
    CHECK_STATE(_dest);
    std::copy(_fbData->begin(), _fbData->end(), _dest.begin());
}

// Function to copy hashes from source to destination
template<typename T, typename U>
void FlatBufferRequest::copyFbHashList(const shared_ptr<vector<ptr<T> > > &_dest, const U &_src, size_t _expectedSize) {
    CHECK_STATE(_src);
    CHECK_STATE(_dest);
    _dest->reserve(_src->size());
    for (const auto &fbHash: *_src) {
        CHECK_STATE(fbHash && fbHash->data() && fbHash->data()->size() == _expectedSize);
        auto hash = make_shared<T>();
        std::copy(fbHash->data()->begin(), fbHash->data()->end(), hash->begin());
        _dest->push_back(hash);
    }
}
