#include "RLP.h"

size_t RLPItem::readLen(
    const std::vector< uint8_t >& _rlp, size_t _offset, size_t _len ) const {
    CHECK_STATE2( _offset + _len <= _rlp.size(), "readLen: out of bounds");
    CHECK_STATE2( _len <= 8, "readLen: length field too large" );
    
    size_t val = 0;
    for ( size_t i = 0; i < _len; ++i ) {
        val = ( val << 8 ) | _rlp.at( _offset + i );
    }
    
    // Check for reasonable size limits
    CHECK_STATE2( val <= MAX_RLP_DATA_SIZE, "readLen: decoded length exceeds maximum allowed size" );
    
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
    _offset += 1;
    CHECK_STATE2( _offset + len <= _rlp.size(), "parseShortByteVector: slice out of bounds" );
    
    std::vector< uint8_t > out( _rlp.begin() + _offset, _rlp.begin() + _offset + len );
    _offset += len;
    m_rawData = std::move(out);
}


void RLPItem::parseLongByteVector(
    const std::vector< uint8_t >& _rlp, uint64_t& _offset, uint8_t _prefix ) {
    size_t lenOfLen = _prefix - 0xb7;

    CHECK_STATE2( _offset + 1 + lenOfLen <= _rlp.size(), "parseLongByteVector: lenOfLen out of bounds" );
    CHECK_STATE2( lenOfLen <= 8, "parseLongByteVector: length-of-length too large" );
    
    size_t len = readLen( _rlp, _offset + 1, lenOfLen );
    _offset += 1 + lenOfLen;

    CHECK_STATE2( _offset + len <= _rlp.size(), "parseLongByteVector: slice out of bounds" );
    CHECK_STATE2( len > 55, "parseLongByteVector: should use short encoding for this length" );
    
    std::vector< uint8_t > out;
    out.reserve(len);
    out.assign( _rlp.begin() + _offset, _rlp.begin() + _offset + len );
    _offset += len;
    m_rawData = std::move(out);
}


void RLPItem::parseLongList(
    const std::vector<uint8_t>& _rlp, uint64_t& offset, uint8_t prefix, size_t depth) {
    
    size_t lenOfLen = prefix - 0xf7;

    CHECK_STATE2( offset + 1 + lenOfLen <= _rlp.size(), "parseLongList: lenOfLen out of bounds");
    CHECK_STATE2( lenOfLen <= 8, "parseLongList: length-of-length too large" );
    
    size_t len = readLen(_rlp, offset + 1, lenOfLen);
    offset += 1 + lenOfLen;
    
    CHECK_STATE2( offset + len <= _rlp.size(), "parseLongList: slice out of bounds");
    CHECK_STATE2( len > 55, "parseLongList: should use short encoding for this length" );
    
    const uint64_t endOffset = offset + len;
    
    // last item from list will be returned - will never be used
    while (offset < endOffset) {
        CHECK_STATE2( children.size() < MAX_RLP_RECURSION_DEPTH, "parseLongList: too many list items (potential DoS)" );
        children.push_back(RLPItem(_rlp, offset, depth + 1));
    }
    CHECK_STATE2(offset == endOffset, "parseLongList: invalid list encoding");   
}


void RLPItem::parseShortList(
    const std::vector<uint8_t>& _rlp, uint64_t& offset, uint8_t prefix, size_t depth) {
    
    size_t len = prefix - 0xc0;
    CHECK_STATE2(offset + 1 + len <= _rlp.size(), "parseShortList: slice out of bounds");
    
    const uint64_t startOffset = offset + 1;
    const uint64_t endOffset = startOffset + len;
    
    offset = startOffset;
    
    // Reserve space to avoid repeated reallocations
    children.reserve(std::min(len, static_cast<size_t>(100))); // Reasonable estimate for short lists
    
    // last item from list will be returned - will never be used
    while (offset < endOffset) {
        CHECK_STATE2( children.size() < MAX_RLP_RECURSION_DEPTH, "parseShortList: too many list items (potential DoS)" );
        children.push_back(RLPItem(_rlp, offset, depth + 1));
    }
    
    CHECK_STATE2(offset == endOffset, "parseShortList: invalid list encoding");   
}

void RLPItem::parseBytes( const std::vector< uint8_t >& _rlp, uint64_t& _offset ) {
    parseBytes(_rlp, _offset, 0);
}

void RLPItem::parseBytes( const std::vector< uint8_t >& _rlp, uint64_t& _offset, size_t _depth ) {

    CHECK_STATE2( _depth < MAX_RLP_RECURSION_DEPTH, "parseBytes: recursion depth exceeded. RLP data has too many nested lists" );

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
        parseShortList(_rlp, _offset, prefix, _depth);
    }
    else {
        parseLongList(_rlp, _offset, prefix, _depth);
    }
}

RLPItem::RLPItem(const std::vector<uint8_t> &data) {
    uint64_t offset = 0;
    parseBytes(data, offset, 0);
}

RLPItem::RLPItem(const std::vector<uint8_t> &data, size_t& offset) {
    parseBytes(data, offset, 0);
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