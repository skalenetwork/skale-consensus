//
// Created by stan on 15-03-2025.
//

#include "SkaleCommon.h"
#include "Log.h"
#include "chains/Schain.h"
#include "FlatBufferRequest.h"

#include "BlockHeaderObject.h"

void block_finalize::FlatBufferRequest::verify(schain_id _sChainId) noexcept( false ) {
    if (schainId != _sChainId) {
        throw std::invalid_argument("Invalid schain:" + to_string(schainId) + "!="
                                    + to_string(_sChainId));
    }
}

static shared_ptr<std::vector<uint8_t> > block_finalize::FlatBufferRequest::copyByteVectorFromFlatBuffer(
    flatbuffers::Vector<uint8_t> *_fbVector) {
    if (!_fbVector) {
        return nullptr;
    } else {
        return make_shared<vector<uint8_t> >(_fbVector->begin(), _fbVector->end());
    }
}

static

template<typename T, typename U>
void block_finalize::FlatBufferRequest::copyFbdata(const T *fb_data, U &dest) {
    CHECK_STATE(fb_data);
    std::copy(fb_data->begin(), fb_data->end(), dest.begin());
}
