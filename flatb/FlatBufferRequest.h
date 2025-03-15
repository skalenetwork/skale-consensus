//
// Created by stan on 15-03-2025.
//

#ifndef SKALED_FLATBUFFERREQUEST_H
#define SKALED_FLATBUFFERREQUEST_H


#include <iostream>

namespace block_finalize {

class FlatBufferRequest {
public:
    FlatBufferRequest( schain_id schainId, epoch_id epochId, block_id blockId, node_id nodeId,
        schain_index proposerIndex )
        : schainId( schainId ),
          epochId( epochId ),
          blockId( blockId ),
          nodeId( nodeId ),
          proposerIndex( proposerIndex ) {}

private:
    schain_id schainId;
    epoch_id epochId;
    block_id blockId;
    node_id nodeId;
    schain_index proposerIndex;
};

}


#define VERIFY_AND_GET_REQUEST(_buffer, RequestType, request) \
    do { \
        static_assert(std::is_pointer_v<decltype(request)>, "Request variable must be a pointer."); \
        flatbuffers::Verifier verifier(_buffer.data(), _buffer.length()); \
        if (!block_finalize::Verify##RequestType##Buffer(verifier)) { \
            throw std::invalid_argument("Invalid FlatBuffer data: verification failed for " #RequestType); \
        } \
        request = flatbuffers::GetRoot<block_finalize::RequestType>(_buffer.data()); \
        if (!request) { \
            throw std::invalid_argument("Invalid FlatBuffer data: failed to parse " #RequestType); \
        } \
    } while (0)

#endif  // SKALED_FLATBUFFERREQUEST_H
