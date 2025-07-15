#pragma once

#include <vector>
#include <cstdint>
#include <SkaleCommon.h>
#include <cstddef>


/**
 * Represents a RLP list of RLP-encoded items.
 * Used to parse RLP data from bytes.
 * 
 * If you need to convert RLP data into vector of bytes, use RLPStream instead.
 */
class RLPItem {
private:
    std::vector<uint8_t> m_rawData; // full raw RLP bytes of this item
    bool m_isList;
    std::vector<RLPItem> children; // only if is_list

    /**
     * Reads length of an RLP long list
     */
    size_t readLen(
        const std::vector< uint8_t >& _rlp, size_t _offset, size_t _len ) const;


    void parseSingleByteVector(
        const std::vector< uint8_t >& _rlp, uint64_t& _offset );

    void parseShortByteVector(
        const std::vector< uint8_t >& _rlp, uint64_t& _offset, uint8_t _prefix ); 

    void parseLongByteVector(
        const std::vector< uint8_t >& _rlp, uint64_t& _offset, uint8_t _prefix );

    void parseLongList(
        const std::vector<uint8_t>& _rlp, uint64_t& offset, uint8_t prefix);

    void parseShortList(
        const std::vector<uint8_t>& _rlp, uint64_t& offset, uint8_t prefix);

    void parseBytes(
        const std::vector< uint8_t >& _rlp, uint64_t& _offset );

public:
    /**
     * @brief Constructs an RLPItem from a raw RLP-encoded byte vector.
     * This should be the base entry point for parsing RLP data.
     */
    RLPItem(const std::vector<uint8_t> &data);

    RLPItem(const std::vector<uint8_t> &data, size_t &offset);

    RLPItem() = default;


    /**
     * Used to set some raw data directly, bypassing the RLP parsing
     */
    RLPItem static fromRawBytes(const std::vector<uint8_t> &data);

    /**
     * Sets the raw data for this RLPItem.
     */
    void setRawData(const std::vector<uint8_t> &data);

    /**
     * Can only be used if the RLPItem is a list.
     */
    RLPItem& operator[](size_t index) {
        CHECK_STATE2(m_isList, "RLP is not a list");
        return children.at(index);
    }

    /**
     * Same as above, but for const RLPItem.
     */
    const RLPItem& operator[](size_t index) const {
        CHECK_STATE2(m_isList, "RLP is not a list");
        return children.at(index);
    }

    bool isList() const;

    size_t size() const {
        return m_isList ? children.size() : 1;
    }

    /**
     * @brief Returns the raw byte data.
     * Can only be used if the RLPItem is not a list.
     */
    const std::vector<uint8_t>& asBytes() const;

   

};



