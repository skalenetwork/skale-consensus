#include "RLPStream.h"
#include <SkaleCommon.h>

/// ------------------ Private methods ------------------ ///

/// ------- RLP encoding methods ------- ///

std::vector< uint8_t > RLPStream::rlpEncodeBytes(const std::vector< uint8_t >& data ) {
    std::vector< uint8_t > out;
    const size_t len = data.size();

    // Explicitly handle empty string
    if ( len == 0 ) {
        out.push_back( 0x80 );
        return out;
    }

    // Single byte < 0x80 is encoded directly (no prefix)
    if ( len == 1 && data[0] < 0x80 ) {
        out.push_back( data[0] );
        return out;
    }

    if ( len < 56 ) {
        // Short string: prefix = 0x80 + len
        out.push_back( static_cast< uint8_t >( 0x80 + len ) );
    } else {
        // Long string: prefix = 0xb7 + len-of-len, then len, then data
        std::vector< uint8_t > len_bytes;
        size_t sz = len;
        while ( sz > 0 ) {
            len_bytes.insert( len_bytes.begin(), static_cast< uint8_t >( sz & 0xFF ) );
            sz >>= 8;
        }

        CHECK_STATE2( len_bytes.size() <= 8, "rlpEncodeBytes: data too large (exceeds 2^64-1)" );

        out.push_back( static_cast< uint8_t >( 0xb7 + len_bytes.size() ) );
        out.insert( out.end(), len_bytes.begin(), len_bytes.end() );
    }

    // Append actual data
    out.insert( out.end(), data.begin(), data.end() );
    return out;
}

std::vector< uint8_t> RLPStream::rlpEncodeUint256(const std::vector< uint8_t >& value ) {
    std::vector< uint8_t > out;
    // Skip leading zeros
    size_t start = 0;
    while ( start < value.size() && value[start] == 0 ) {
        ++start;
    }

    // Extract minimal non-zero representation (or empty)
    std::vector< uint8_t > stripped( value.begin() + start, value.end() );

    // Delegate to rlp_encode_bytes
    return rlpEncodeBytes( stripped );
}


/// ------------------ Public methods ------------------ ///


/// ------- Byte-type Conversion methods ------- ///

std::vector< uint8_t> RLPStream::u256toBytes( u256 v_value ) {
    std::vector<uint8_t> bytes(32);
    for (size_t i = 0; i < 32; i++) {
        bytes[31 - i] = static_cast<uint8_t>(v_value & 0xFF);
        v_value >>= 8;
    }
    return bytes;
}

u256 RLPStream::bytesToU256(const std::vector<uint8_t>& bytes) {
    u256 val = 0;
    size_t bytes_size = bytes.size();
    
    for (size_t i = 0; i < bytes_size; ++i) {
        if (i >= 32) {
            // If more than 32 bytes, ignore the rest
            break;
        }
        val = (val << 8) | bytes[i];
    }
    return val;
}

std::vector<uint8_t> RLPStream::encode() const {
    std::vector< uint8_t > out;
    std::vector< uint8_t > payload;
    for ( const auto& e : data ) {
        payload.insert( payload.end(), e.begin(), e.end() );
    }
    if ( payload.size() < 56 ) {
        out.push_back( 0xc0 + payload.size() );
    } else {
        std::vector< uint8_t > len;
        size_t sz = payload.size();
        while ( sz ) {
            len.insert( len.begin(), static_cast< uint8_t >( sz & 0xFF ) );
            sz >>= 8;
        }
        out.push_back( 0xf7 + len.size() );
        out.insert( out.end(), len.begin(), len.end() );
    }
    out.insert( out.end(), payload.begin(), payload.end() );
    return out;
}
