#pragma once

#include <vector>
#include <cstdint>
#include <SkaleCommon.h>
#include <cstddef>

// Security constants to prevent attacks
static constexpr size_t MAX_RLP_DATA_SIZE = 64 * 1024 * 1024; // 64 MB max total data
static constexpr size_t MAX_RLP_LIST_LENGTH = 1024 * 1024;    // 1M max list items
static constexpr size_t MAX_RLP_NESTING_DEPTH = 1024;         // Max recursion depth
static constexpr size_t MAX_RLP_ITEM_SIZE = 32 * 1024 * 1024; // 32 MB max single item
static constexpr size_t MAX_LENGTH_BYTES = 8;                 // Max bytes for length encoding

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
    
    // Security: track parsing depth to prevent stack overflow
    mutable size_t m_nestingDepth = 0;

    /**
     * Reads length of an RLP long list
     */
    size_t readLen(
        const std::vector< uint8_t >& _rlp, size_t _offset, size_t _len ) const;

    // Security validation methods
    void validateDataSize(size_t size) const;
    void validateListLength(size_t length) const;
    void validateNestingDepth(size_t depth) const;
    void validateItemSize(size_t size) const;
    void validateLengthBytes(size_t lengthBytes) const;

    void parseSingleByteVector(
        const std::vector< uint8_t >& _rlp, uint64_t& _offset );

    void parseShortByteVector(
        const std::vector< uint8_t >& _rlp, uint64_t& _offset, uint8_t _prefix ); 

    void parseLongByteVector(
        const std::vector< uint8_t >& _rlp, uint64_t& _offset, uint8_t _prefix );
    
    // Depth-aware parsing methods to prevent stack overflow
    void parseShortListWithDepth(
        const std::vector<uint8_t>& _rlp, uint64_t& offset, uint8_t prefix, size_t depth);
    
    void parseLongListWithDepth(
        const std::vector<uint8_t>& _rlp, uint64_t& offset, uint8_t prefix, size_t depth);
    
    void parseBytesWithDepth(
        const std::vector< uint8_t >& _rlp, uint64_t& _offset, size_t depth );

public:
    /**
     * @brief Constructs an RLPItem from a raw RLP-encoded byte vector.
     * This should be the base entry point for parsing RLP data.
     * @throws InvalidStateException if data exceeds security limits
     */
    RLPItem(const std::vector<uint8_t> &data);

    /**
     * @brief Constructs an RLPItem from a raw RLP-encoded byte vector with offset.
     * @throws InvalidStateException if data exceeds security limits
     */
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



