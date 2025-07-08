#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>
#include <boost/multiprecision/cpp_int.hpp>

using uint256 = std::vector< uint8_t >;

using u256 = boost::multiprecision::number< boost::multiprecision::cpp_int_backend< 256, 256,
boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void > >;

/**
 * @brief RLPStream is a class for encoding data in the Recursive Length Prefix (RLP) format.
 * It is only used for encoding, not decoding.
 * For decoding, refer to the RLP class.
 */
class RLPStream {
private:

    //  ----------------------- Helper functions for RLP encoding -------------------------------//

    static std::vector< uint8_t > rlpEncodeBytes( const std::vector< uint8_t >& data );

    static std::vector< uint8_t > rlpEncodeUint256( const std::vector< uint8_t >& value );

public:
    RLPStream() = default;

    std::vector<std::vector<uint8_t>> data;

    /**
     * @brief Convert a u256 value to a vector of bytes in big-endian order.
     */
    static std::vector< uint8_t> u256toBytes( u256 v_value );

    /**
     * @brief Convert a vector of bytes to a u256 value in big-endian order.
     */
    static u256 bytesToU256(const std::vector<uint8_t>& bytes);

    std::vector<uint8_t> encode() const;

    // Appending operators
    RLPStream& operator<<(const std::vector<uint8_t>& bytes) {
        data.push_back(rlpEncodeBytes(bytes));
        return *this;
    }

    RLPStream& operator<<(const u256& value) {
        data.push_back(rlpEncodeUint256(u256toBytes(value)));
        return *this;
    }

    RLPStream& operator<<(const RLPStream& other) {
        data.push_back(other.encode());
        return *this;
    }




};