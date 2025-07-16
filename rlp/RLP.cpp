#include "RLP.h"

// ----------- Security validation methods ---------------- //

void RLPItem::validateDataSize(size_t size) const {
    CHECK_STATE2(size <= MAX_RLP_DATA_SIZE, 
        "RLP data size " + std::to_string(size) + " exceeds maximum " + std::to_string(MAX_RLP_DATA_SIZE));
}

void RLPItem::validateListLength(size_t length) const {
    CHECK_STATE2(length <= MAX_RLP_LIST_LENGTH,
        "RLP list length " + std::to_string(length) + " exceeds maximum " + std::to_string(MAX_RLP_LIST_LENGTH));
}

void RLPItem::validateNestingDepth(size_t depth) const {
    CHECK_STATE2(depth <= MAX_RLP_NESTING_DEPTH,
        "RLP nesting depth " + std::to_string(depth) + " exceeds maximum " + std::to_string(MAX_RLP_NESTING_DEPTH));
}

void RLPItem::validateItemSize(size_t size) const {
    CHECK_STATE2(size <= MAX_RLP_ITEM_SIZE,
        "RLP item size " + std::to_string(size) + " exceeds maximum " + std::to_string(MAX_RLP_ITEM_SIZE));
}

void RLPItem::validateLengthBytes(size_t lengthBytes) const {
    CHECK_STATE2(lengthBytes <= MAX_LENGTH_BYTES,
        "RLP length encoding " + std::to_string(lengthBytes) + " bytes exceeds maximum " + std::to_string(MAX_LENGTH_BYTES));
}


// ----------- Parsing methods ---------------- //

size_t RLPItem::readLen(
    const std::vector< uint8_t >& _rlp, size_t _offset, size_t _len ) const {
    CHECK_STATE2( _offset + _len <= _rlp.size(), "readLen: out of bounds");
    
    // Security: validate length encoding size
    validateLengthBytes(_len);
    
    size_t val = 0;
    for ( size_t i = 0; i < _len; ++i ) {
        // Security: check for integer overflow
        CHECK_STATE2(val <= (SIZE_MAX >> 8), "readLen: integer overflow in length calculation");
        val = ( val << 8 ) | _rlp.at( _offset + i );
    }
    
    // Security: validate computed length
    validateItemSize(val);
    
    return val;
}

void RLPItem::parseSingleByteVector(
    const std::vector< uint8_t >& _rlp, uint64_t& _offset ) {
    CHECK_STATE2( _offset < _rlp.size(), "Short element: out of bounds" );
    m_rawData = { _rlp.at( _offset++ ) };
}

void RLPItem::parseShortByteVector(
    const std::vector< uint8_t >& _rlp, uint64_t& _offset, uint8_t _prefix ) {
    size_t len = _prefix - 0x80;
    
    // Security: validate item size
    validateItemSize(len);
    
    _offset += 1;
    CHECK_STATE2( _offset + len <= _rlp.size(), "parseShortByteVector: slice out of bounds" );
    
    std::vector< uint8_t > out( _rlp.begin() + _offset, _rlp.begin() + _offset + len );
    _offset += len;
    m_rawData = std::move(out);
}


void RLPItem::parseLongByteVector(
    const std::vector< uint8_t >& _rlp, uint64_t& _offset, uint8_t _prefix ) {
    size_t lenOfLen = _prefix - 0xb7;

    // Security: validate length encoding size
    validateLengthBytes(lenOfLen);
    
    CHECK_STATE2( _offset + 1 + lenOfLen <= _rlp.size(), "parseLongByteVector: lenOfLen out of bounds" );
    
    size_t len = readLen( _rlp, _offset + 1, lenOfLen );
    
    // Security: additional validation for long byte vectors
    CHECK_STATE2(len >= 56, "parseLongByteVector: length must be >= 56 for long encoding");
    
    _offset += 1 + lenOfLen;

    CHECK_STATE2( _offset + len <= _rlp.size(), "parseLongByteVector: slice out of bounds" );
    
    std::vector< uint8_t > out( _rlp.begin() + _offset, _rlp.begin() + _offset + len );
    _offset += len;
    m_rawData = std::move(out);
}

void RLPItem::parseShortListWithDepth(
    const std::vector<uint8_t>& _rlp, uint64_t& offset, uint8_t prefix, size_t depth) {
    
    size_t len = prefix - 0xc0;
    CHECK_STATE2(offset + 1 + len <= _rlp.size(), "parseShortListWithDepth: slice out of bounds");
    
    const uint64_t startOffset = offset + 1;
    const uint64_t endOffset = startOffset + len;
    
    offset = startOffset;
    size_t itemCount = 0;
    
    // Security: validate list won't exceed maximum items
    while (offset < endOffset) {
        CHECK_STATE2(itemCount < MAX_RLP_LIST_LENGTH, 
            "parseShortListWithDepth: list item count exceeds maximum");
        
        RLPItem child;
        child.parseBytesWithDepth(_rlp, offset, depth);
        children.push_back(std::move(child));
        itemCount++;
    }
    
    CHECK_STATE2(offset == endOffset, "parseShortListWithDepth: invalid list encoding");   
}

void RLPItem::parseLongListWithDepth(
    const std::vector<uint8_t>& _rlp, uint64_t& offset, uint8_t prefix, size_t depth) {
    
    size_t lenOfLen = prefix - 0xf7;

    // Security: validate length encoding size
    validateLengthBytes(lenOfLen);

    CHECK_STATE2( offset + 1 + lenOfLen <= _rlp.size(), "parseLongListWithDepth: lenOfLen out of bounds");
    
    size_t len = readLen(_rlp, offset + 1, lenOfLen);
    
    // Security: additional validation for long lists
    CHECK_STATE2(len >= 56, "parseLongListWithDepth: length must be >= 56 for long encoding");
    
    offset += 1 + lenOfLen;
    
    CHECK_STATE2( offset + len <= _rlp.size(), "parseLongListWithDepth: slice out of bounds");
    
    const uint64_t endOffset = offset + len;
    size_t itemCount = 0;
    
    // Security: validate list won't exceed maximum items
    while (offset < endOffset) {
        CHECK_STATE2(itemCount < MAX_RLP_LIST_LENGTH, 
            "parseLongListWithDepth: list item count exceeds maximum");
        
        RLPItem child;
        child.parseBytesWithDepth(_rlp, offset, depth);
        children.push_back(std::move(child));
        itemCount++;
    }
    CHECK_STATE2(offset == endOffset, "parseLongListWithDepth: invalid list encoding");   
}

void RLPItem::parseBytesWithDepth( const std::vector< uint8_t >& _rlp, uint64_t& _offset, size_t depth ) {

    // Security: validate nesting depth to prevent stack overflow
    validateNestingDepth(depth);

    m_nestingDepth = depth;

    if ( _offset >= _rlp.size() ) return;

    uint8_t prefix = _rlp.at( _offset );
    m_isList = prefix > 0xbf;

    if ( prefix <= 0x7f ){
        parseSingleByteVector( _rlp, _offset );
    }
    else if ( prefix <= 0xb7 ) {
        parseShortByteVector( _rlp, _offset, prefix );
    }
    else if ( prefix <= 0xbf ) {
        parseLongByteVector( _rlp, _offset, prefix );
    }
    else if (prefix <= 0xf7) {
        parseShortListWithDepth(_rlp, _offset, prefix, depth + 1);
    }
    else {
        parseLongListWithDepth(_rlp, _offset, prefix, depth + 1);
    }
}

RLPItem::RLPItem(const std::vector<uint8_t> &data) {
    // Security: validate total data size
    validateDataSize(data.size());
    
    uint64_t offset = 0;
    parseBytesWithDepth(data, offset, 0);
}

RLPItem::RLPItem(const std::vector<uint8_t> &data, size_t& offset) {
    // Security: validate total data size
    validateDataSize(data.size() - offset);
    parseBytesWithDepth(data, offset, 0);
}

bool RLPItem::isList() const {
    return m_isList;
}

const std::vector<uint8_t>& RLPItem::asBytes() const {
    CHECK_STATE2(!m_isList, "RLPItem is a list, cannot return asBytes");
    return m_rawData;
}

RLPItem RLPItem::fromRawBytes(const std::vector<uint8_t> &data) {
    auto rlp = RLPItem();
    rlp.m_rawData = data;
    return rlp;
}

void RLPItem::setRawData(const std::vector<uint8_t> &data) {
    m_rawData = data;
    m_isList = false; // reset the list state
    children.clear(); // clear any previous children
}