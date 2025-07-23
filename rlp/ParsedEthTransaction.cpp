
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


#include "SkaleCommon.h"
#include "Log.h"

#include "rlp/ParsedEthTransaction.h"
#include "EthTransactionEncoder.h"
#include "ParsedEthTransaction.h"
#include "EthTransaction.h"
#include "RLP.h"


bool ParsedEthTransaction::isTypedTransaction( uint8_t prefix ) {
    return prefix == 0x01 || prefix == 0x02;
}

ptr< ParsedEthTransaction > ParsedEthTransaction::parse( const std::vector< uint8_t >& _rawTx ) {
    CHECK_STATE2( !_rawTx.empty(), "Transaction data is empty" );

    auto result = make_shared< ParsedEthTransaction >();
    size_t offset = 0;
    uint8_t prefix = _rawTx.at( offset );

    if ( isTypedTransaction( prefix ) ) {
        result->type = prefix;
        offset += 1;
        int fieldCount = ( prefix == 0x01 ) ? 11 : 12;
        RLPItem rlpItem( _rawTx, offset );
        CHECK_STATE2( rlpItem.isList(), "RLP item is not a list" );
        CHECK_STATE2( rlpItem.size() == fieldCount, "Expected " + std::to_string(fieldCount) +
                      " fields, found: " + std::to_string(rlpItem.size()) );
        result->fields = std::move(rlpItem);
    } else if ( prefix >= 0xc0 ) {
        result->type = 0;
        RLPItem rlpItem( _rawTx, offset );
        CHECK_STATE2( rlpItem.isList(), "RLP item is not a list" );
        CHECK_STATE2( rlpItem.size() == 9, "Expected " + std::to_string(9) +
                      " fields, found: " + std::to_string(rlpItem.size()) );
        result->fields = std::move(rlpItem);
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

    CHECK_STATE2( fields.size() == expectedFields, "Expected " + std::to_string(expectedFields) +
                  " fields, found: " + std::to_string(fields.size()) );

}
void ParsedEthTransaction::validateToField() {
    // if type 0, then idx is 3
    // if type 1, then idx is 4
    // if type 2, then idx is 5
    auto toFieldIdx = 3 + type;

    const auto toField = fields[ toFieldIdx ].asBytes();
    // toField is empty when deploying a contract
    CHECK_STATE2( toField.empty() || toField.size() == 20, "Invalid 'to' address length");
}

inline bool ParsedEthTransaction::isZero( const std::vector< uint8_t >& _data ) {
    for ( uint8_t byte : _data ) {
        if ( byte != 0 )
            return false;
    }
    return true;
}

std::vector< uint8_t > padTo32Bytes( const std::vector< uint8_t >& input ) {
    constexpr size_t TARGET_SIZE = 32;
    std::vector< uint8_t > result( TARGET_SIZE, 0 );  // initialize with zeros

    CHECK_STATE( input.size() <= TARGET_SIZE );
    // Copy input to the rightmost part of result
    std::copy( input.begin(), input.end(), result.begin() + ( TARGET_SIZE - input.size() ) );

    return result;
}

void ParsedEthTransaction::validateSignature() {
    const auto v = fields[ fields.size() - 3 ].asBytes();
    const auto r = fields[ fields.size() - 2 ].asBytes();
    const auto s = fields[ fields.size() - 1 ].asBytes();

    CHECK_STATE2( r.size() <= 32, "Invalid r size. Should be <= 32 bytes, found: " + std::to_string(r.size()) );
    CHECK_STATE2( s.size() <= 32, "Invalid s size. Should be <= 32 bytes, found: " + std::to_string(s.size()) );
    CHECK_STATE2( !isZero( r ), "r is zero" );
    CHECK_STATE2( !isZero( s ), "s is zero" );
    CHECK_STATE2( type < 3, "Invalid transaction type: " + std::to_string(type) );

    std::unique_ptr<EthTransaction> txWithoutSig;
    
    TxType txType = static_cast< TxType >( type );
    switch (txType) {
        case TxType::LEGACY:
            txWithoutSig = std::make_unique<LegacyTx>(fields);
            break;
        case TxType::TYPE1:
            txWithoutSig = std::make_unique<Type1Tx>(fields);
            break;
        case TxType::TYPE2:
            txWithoutSig = std::make_unique<Type2Tx>(fields);
            break;
        default:
            throw invalid_argument( "Unknown transaction type" );
    }

    EthTransaction& txWithoutSigRef = *txWithoutSig;

    auto r_padded = padTo32Bytes( r );
    auto s_padded = padTo32Bytes( s );
    Signature sig(v, r_padded, s_padded);

    txWithoutSigRef.verifySignature( sig );

}

ptr< std::vector< uint8_t > > ParsedEthTransaction::getToField() const {
    if ( !hasToField() ) {
        throw invalid_argument( "Transaction missing to field" );
    }
    return make_shared< vector< uint8_t > >( fields[ getToFieldIndex() ].asBytes() );
}

bool ParsedEthTransaction::hasToField() const {
    size_t index = getToFieldIndex();
    return fields.size() > index && !fields[index].asBytes().empty();    
}

ptr< std::vector< uint8_t > > ParsedEthTransaction::getTransactionDataField() {
    size_t index = getDataFieldIndex();
    CHECK_STATE2( fields.size() > index, "Transaction missing data field" );
    return make_shared< vector< uint8_t > >( fields[ index ].asBytes() );
}

void ParsedEthTransaction::setTransactionDataField(vector<uint8_t>&  _dataField) {
    size_t index = getDataFieldIndex();
    CHECK_STATE2( fields.size() > index, "Transaction missing data field" );
    fields[ index ].setRawData(_dataField);
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
const RLPItem& ParsedEthTransaction::getFields() const {
    return fields;
}
uint8_t ParsedEthTransaction::getType() const {
    return type;
}

/// -------------------------------------------
///             Helper Functions
/// -------------------------------------------

size_t ParsedEthTransaction::getToFieldIndex() const {
    // If Legacy -> 3 + 0 = 3
    // Type 1    -> 3 + 1 = 4
    // Type 2    -> 3 + 2 = 5
    return 3 + type;
}

size_t ParsedEthTransaction::getDataFieldIndex() const {
    // If Legacy -> 5 + 0 = 5
    // Type 1    -> 5 + 1 = 6
    // Type 2    -> 5 + 2 = 7
    return 5 + type;
}
