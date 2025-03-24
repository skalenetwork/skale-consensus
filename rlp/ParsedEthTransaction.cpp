#include "SkaleCommon.h"

#include "ParsedEthTransaction.h"


#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/bn.h>
#include <iostream>
#include <vector>
#include <random>
#include <cstring>
#include <variant>
#include <optional>
#include <sstream>
#include <array>


bool ParsedEthTransaction::isTypedTransaction( uint8_t prefix ) {
    return prefix == 0x01 || prefix == 0x02;
}

size_t ParsedEthTransaction::readLen(
    const std::vector< uint8_t >& _tx, size_t _offset, size_t _len ) {
    if ( _offset + _len > _tx.size() ) {
        throw std::runtime_error( "readLen: out of bounds" );
    }
    size_t val = 0;
    for ( size_t i = 0; i < _len; ++i ) {
        val = ( val << 8 ) | _tx.at( _offset + i );
    }
    return val;
}

void ParsedEthTransaction::skipRlpListHeader( const std::vector< uint8_t >& _tx, uint64_t& _offset ) {
    if ( _offset >= _tx.size() )
        throw std::runtime_error( "skipRlpListHeader: no bytes left" );
    uint8_t prefix = _tx.at( _offset );
    if ( prefix <= 0xf7 ) {
        _offset += 1;
    } else if ( prefix <= 0xff ) {
        size_t lenOfLen = prefix - 0xf7;
        if ( _offset + 1 + lenOfLen > _tx.size() ) {
            throw std::runtime_error( "skipRlpListHeader: out of bounds" );
        }
        _offset += 1 + lenOfLen;
    } else {
        throw invalid_argument( "Invalid RLP list prefix" );
    }
}

std::vector< uint8_t > ParsedEthTransaction::parseSingleByteVector(
    const std::vector< uint8_t >& _tx, uint64_t& _offset ) {
    if ( _offset >= _tx.size() )
        throw std::runtime_error( "Short element: out of bounds" );
    return { _tx.at( _offset++ ) };
}

std::vector< uint8_t > ParsedEthTransaction::parseShortByteVector(
    const std::vector< uint8_t >& _tx, uint64_t& _offset, uint8_t _prefix ) {
    size_t len = _prefix - 0x80;
    _offset += 1;
    if ( _offset + len > _tx.size() ) {
        throw std::runtime_error( "parseShortByteVector: slice out of bounds" );
    }
    std::vector< uint8_t > out( _tx.begin() + _offset, _tx.begin() + _offset + len );
    _offset += len;
    return out;
}


std::vector< uint8_t > ParsedEthTransaction::parseLongByteVector(
    const std::vector< uint8_t >& _tx, uint64_t& _offset, uint8_t _prefix ) {
    size_t lenOfLen = _prefix - 0xb7;
    if ( _offset + 1 + lenOfLen > _tx.size() ) {
        throw std::runtime_error( "parseLongByteVector: lenOfLen out of bounds" );
    }
    size_t len = readLen( _tx, _offset + 1, lenOfLen );
    _offset += 1 + lenOfLen;
    if ( _offset + len > _tx.size() ) {
        throw std::runtime_error( "parseLongByteVector: slice out of bounds" );
    }
    std::vector< uint8_t > out( _tx.begin() + _offset, _tx.begin() + _offset + len );
    _offset += len;
    return out;
}

std::vector< uint8_t > ParsedEthTransaction::parseByteVector(
    const std::vector< uint8_t >& _tx, uint64_t& _offset ) {
    if ( _offset >= _tx.size() )
        throw std::runtime_error( "parseByteVector: no data left" );
    uint8_t prefix = _tx.at( _offset );
    if ( prefix <= 0x7f )
        return parseSingleByteVector( _tx, _offset );
    else if ( prefix <= 0xb7 )
        return parseShortByteVector( _tx, _offset, prefix );
    else if ( prefix <= 0xbf )
        return parseLongByteVector( _tx, _offset, prefix );
    else
        throw invalid_argument( "Invalid RLP element prefix" );
}

void ParsedEthTransaction::parseTransactionFields(
    const std::vector< uint8_t >& _tx, size_t& _offset, int fieldCount ) {
    for ( int i = 0; i < fieldCount; ++i ) {
        fields.push_back( parseByteVector( _tx, _offset ) );
    }

    if ( _offset != _tx.size() ) {
        throw invalid_argument( "Too many fields in transaction" );
    }
}

ptr< ParsedEthTransaction > ParsedEthTransaction::parse( const std::vector< uint8_t >& _rawTx ) {
    if ( _rawTx.empty() ) {
        throw invalid_argument( "Empty transaction" );
    }

    auto result = make_shared< ParsedEthTransaction >();
    size_t offset = 0;
    uint8_t prefix = _rawTx.at( offset );

    if ( isTypedTransaction( prefix ) ) {
        result->type = prefix;
        offset += 1;
        skipRlpListHeader( _rawTx, offset );
        int fieldCount = ( prefix == 0x01 ) ? 11 : 12;
        result->parseTransactionFields( _rawTx, offset, fieldCount );
    } else if ( prefix >= 0xc0 ) {
        result->type = 0;
        skipRlpListHeader( _rawTx, offset );
        result->parseTransactionFields( _rawTx, offset, 9 );
    } else {
        throw invalid_argument( "Invalid transaction prefix" );
    }

    result->validateAll();

    return result;
}

void ParsedEthTransaction::validateAll() {
    validateFieldsCount();
    validateToField();
    validateSignature();
}
void ParsedEthTransaction::validateFieldsCount() const {
    size_t expectedFields = 0;
    switch ( type ) {
    case 0:
        expectedFields = 9;
        break;
    case 1:
        expectedFields = 11;
        break;
    case 2:
        expectedFields = 12;
        break;
    default:
        throw invalid_argument( "Unknown transaction type" );
    }
    if ( fields.size() != expectedFields ) {
        throw invalid_argument( "Incorrect number of fields" );
    }
}
void ParsedEthTransaction::validateToField() {
    const auto& toField = fields.at( type == 0 ? 3 : 5 );
    if ( !toField.empty() && toField.size() != 20 ) {
        throw invalid_argument( "Invalid 'to' address length" );
    }
}

inline bool ParsedEthTransaction::isZero( const std::vector< uint8_t >& _data ) {
    for ( uint8_t byte : _data ) {
        if ( byte != 0 )
            return false;
    }
    return true;
}

void ParsedEthTransaction::validateSignature() {
    const auto& r = fields.at( fields.size() - 2 );
    const auto& s = fields.at( fields.size() - 1 );
    if ( r.size() > 32 || s.size() > 32 ) {
        throw invalid_argument( "Invalid r/s size (should be <= 32 bytes)" );
    }

    if ( isZero( r ) || isZero( s ) ) {
        throw invalid_argument( "Zero r/s" );
    }
}

ptr< std::vector< uint8_t > > ParsedEthTransaction::getTransactionDataField() {
    size_t index;
    switch ( type ) {
    case 0:
        index = 5;
        break;
    case 1:
    case 2:
        index = 7;
        break;
    default:
        throw invalid_argument( "Unknown transaction type" );
    }
    if ( fields.size() <= index ) {
        throw invalid_argument( "Transaction missing data field" );
    }
    return make_shared< vector< uint8_t > >( fields.at( index ) );
}


void ParsedEthTransaction::testEthereumTxParser() {
    std::vector< std::vector< uint8_t > > testTxs = { // Legacy transaction (valid)
        { 0xf8, 0x66, 0x82, 0x01, 0xf4, 0x84, 0x3b, 0x9a, 0xca, 0x00, 0x83, 0x01, 0x86, 0xa0, 0x94,
            0xd8, 0x94, 0xd9, 0x96, 0x83, 0xb2, 0x74, 0xc3, 0x86, 0xee, 0xe2, 0x7b, 0xb1, 0x17,
            0xf1, 0x99, 0x01, 0x80, 0x1b, 0xa0, 0x6e, 0x87, 0x69, 0xb0, 0x90, 0x8e, 0x0f, 0xd6,
            0x46, 0x8d, 0xa0, 0x69, 0x85, 0x39, 0x6f, 0xf6, 0x77, 0x6b, 0x2a, 0xb7, 0xb3, 0x5f,
            0x82, 0xe9, 0xb6, 0xb3, 0x47, 0xea, 0x1b, 0xa0, 0x52, 0x99, 0x55, 0xaa, 0x99, 0x9c,
            0x35, 0x13, 0x39, 0x44, 0xfa, 0x73, 0xe2, 0xdd, 0x7d, 0x4b, 0xd3, 0x0d, 0x6b, 0x30,
            0xa3, 0xc4, 0x5f, 0x3e, 0xf6, 0x44, 0x1d, 0x6d, 0x89, 0xa6 },

        // EIP-2930 transaction (valid)
        { 0x01, 0xf8, 0x44, 0x82, 0x01, 0xf4, 0x84, 0x3b, 0x9a, 0xca, 0x00, 0x83, 0x01, 0x86, 0xa0,
            0x94, 0xd8, 0x94, 0xd9, 0x96, 0x83, 0xb2, 0x74, 0xc3, 0x86, 0xee, 0xe2, 0x7b, 0xb1,
            0x17, 0xf1, 0x99, 0x01, 0x80, 0xc0, 0x1b, 0xa0, 0x6e, 0x87, 0x69, 0xb0, 0x90, 0x8e,
            0x0f, 0xd6, 0x46, 0x8d, 0xa0, 0x69, 0x85, 0x39, 0x6f, 0xf6, 0x77, 0x6b, 0x2a, 0xb7,
            0xb3, 0x5f, 0x82, 0xe9, 0xb6, 0xb3, 0x47, 0xea, 0x1b },

        // EIP-1559 transaction (valid)
        { 0x02, 0xf8, 0x4a, 0x01, 0x85, 0x04, 0xa8, 0x1e, 0x00, 0x85, 0x04, 0xa8, 0x1e, 0x00, 0x82,
            0x01, 0xf4, 0x94, 0xd8, 0x94, 0xd9, 0x96, 0x83, 0xb2, 0x74, 0xc3, 0x80, 0xc0, 0x1b,
            0xa0, 0x6e, 0x87, 0x69, 0xb0, 0x90, 0x8e, 0x0f, 0xd6, 0x46, 0x8d, 0xa0, 0x69, 0x85,
            0x39, 0x6f, 0xf6, 0x77, 0x6b, 0x2a, 0xb7, 0xb3, 0x5f, 0x82, 0xe9, 0xb6, 0xb3, 0x47,
            0xea, 0x1b },

        // Malformed (bad) transaction - truncated
        { 0xf8, 0x02, 0x82 } };

    for ( size_t i = 0; i < testTxs.size(); ++i ) {
        try {
            auto tx = parse( testTxs[i] );
        } catch ( const std::exception& e ) {
            std::cout << "Transaction " << i << ": parse failed - " << e.what() << "\n";
        }
    }
}

