//
// Created by stan on 15-03-2025.
//

#include "SkaleCommon.h"
#include "Log.h"
#include "chains/Schain.h"
#include "FlatBufferRequest.h"
void block_finalize::FlatBufferRequest::verify( schain_id _sChainId ) noexcept( false ) {
    if (schainId != _sChainId) {
        throw std::invalid_argument("Invalid schain:" + to_string(schainId) + "!="
            + to_string(_sChainId));
    }
}
