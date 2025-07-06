#pragma once
#include <vector>
#include <cstdint>
#include <boost/multiprecision/cpp_int.hpp>

using uint256 = std::vector< uint8_t >;

using u256 = boost::multiprecision::number< boost::multiprecision::cpp_int_backend< 256, 256,
boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void > >;

class RLPStream {
private:

    //  ----------------------- Helper functions for RLP encoding -------------------------------//

    inline static std::vector< uint8_t > rlpEncodeBytes( const std::vector< uint8_t >& data );

    inline static std::vector< uint8_t > rlpEncodeUint256( const std::vector< uint8_t >& value );

    inline static std::vector< uint8_t > rlpEncodeList( const std::vector< std::vector< uint8_t > >& elements );

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

    RLPStream& operator<<(const std::vector<uint8_t>& bytes) {
        data.push_back(rlpEncodeBytes(bytes));
        return *this;
    }

    RLPStream& operator<<(const u256& value) {
        data.push_back(rlpEncodeUint256(u256toBytes(value)));
        return *this;
    }

    RLPStream& operator<<(const RLPStream& other) {
        for (const auto& e : other.data) {
            data.push_back(e);
        }
        return *this;
    }
};