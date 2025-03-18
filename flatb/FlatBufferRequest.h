//
// Created by stan on 15-03-2025.
//

#ifndef SKALED_FLATBUFFERREQUEST_H
#define SKALED_FLATBUFFERREQUEST_H


#include <iostream>

#include "block_finalize_common_structures_generated.h"

class Schain;


namespace block_finalize {
    class BlockFragment;

    class FlatBufferRequest {
    public:
        FlatBufferRequest(schain_id schainId, epoch_id epochId, block_id blockId, node_id nodeId,
                          schain_index proposerIndex)
            : schainId(schainId),
              epochId(epochId),
              blockId(blockId),
              nodeId(nodeId),
              proposerIndex(proposerIndex) {
        }

    private:
        schain_id schainId;
        epoch_id epochId;
        block_id blockId;
        node_id nodeId;
        schain_index proposerIndex;

    protected:
        void verify(schain_id _sChainId) noexcept( false );

    public:
        static shared_ptr<std::vector<uint8_t> > copyByteVectorFromFlatBuffer(flatbuffers::Vector<uint8_t> *_fbVector);

        static shared_ptr<BlockFragment> copyBlockHeaderFromFlatBuffer(block_finalize::BlockFragment *_blockFragment);

        template<typename T, typename U> static
        void copyFbdata(const T *fb_data, U &dest);
    };
} // namespace block_finalize


#define VERIFY_AND_PARSE_FLATBUFFER( _buffer, FlatBufferType, request )                                \
    do {                                                                                       \
        static_assert(                                                                         \
            std::is_pointer_v< decltype( request ) >, "Request variable must be a pointer." ); \
        flatbuffers::Verifier verifier( _buffer.data(), _buffer.length() );                    \
        if ( !block_finalize::Verify##FlatBufferType##Buffer( verifier ) ) {                      \
            throw std::invalid_argument(                                                       \
                "Invalid FlatBuffer data: verification failed for " #FlatBufferType );            \
        }                                                                                      \
        request = flatbuffers::GetRoot< block_finalize::FlatBufferType >( _buffer.data() );       \
        if ( !request ) {                                                                      \
            throw std::invalid_argument(                                                       \
                "Invalid FlatBuffer data: failed to parse " #FlatBufferType );                    \
        }                                                                                      \
    } while ( 0 )

#endif  // SKALED_FLATBUFFERREQUEST_H
