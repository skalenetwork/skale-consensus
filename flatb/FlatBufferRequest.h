#pragma once

#include <iostream>

#include "flatbuffers/flatbuffers.h"



namespace skale_fb {

    class FlatBufferRequest {

    private:
        schain_id schainId;
        epoch_id epochId;
        block_id blockId;
        node_id nodeId;
        schain_index proposerIndex;


    public:


        FlatBufferRequest(schain_id _schainId, epoch_id epochId, block_id _blockId, node_id _nodeId,
                          schain_index _proposerIndex);

        template<typename T, typename U> static
        void copyFbArray(const T *_fbData, U &_dest);


        // Function to copy hashes from source to destination
        template <typename T, typename U> static
        void copyFbHashList(const shared_ptr<vector<ptr<T>>>& _dest, const U& _src, size_t _expectedSize);

        static shared_ptr<std::vector<uint8_t> > copyFbByteVector(const flatbuffers::Vector<uint8_t> *_fbVector);
        static shared_ptr<std::vector<transaction_index> > copyFbIndexVector(const flatbuffers::Vector<uint16_t> *_fbVector);




    };
} // namespace block_finalize


#define VERIFY_AND_PARSE_FLATBUFFER( _buffer, FlatBufferType, request )                                \
    do {                                                                                       \
        static_assert(                                                                         \
            std::is_pointer_v< decltype( request ) >, "Request variable must be a pointer." ); \
        flatbuffers::Verifier verifier( _buffer.data(), _buffer.length() );                    \
        if ( !skale_fb::Verify##FlatBufferType##Buffer( verifier ) ) {                      \
            throw std::invalid_argument(                                                       \
                "Invalid FlatBuffer data: verification failed for " #FlatBufferType );            \
        }                                                                                      \
        request = flatbuffers::GetRoot< skale_fb::FlatBufferType >( _buffer.data() );       \
        if ( !request ) {                                                                      \
            throw std::invalid_argument(                                                       \
                "Invalid FlatBuffer data: failed to parse " #FlatBufferType );                    \
        }                                                                                      \
    } while ( 0 )

