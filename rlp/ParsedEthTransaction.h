#pragma once

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <string>

class ParsedEthTransaction {
    uint8_t type = 0;  // 0 = legacy
    std::vector< std::vector< uint8_t > > fields;


    inline void parseTransactionFields( const std::vector< uint8_t >& _tx, size_t& _offset, int fieldCount);
    inline void validateAll();
    inline void validateSignature();
    inline bool isZero(const std::vector<uint8_t>& _data );

    static inline std::vector< uint8_t > parseSingleByteVector(
        const std::vector< uint8_t >& _tx, size_t& _offset );
    static inline std::vector< uint8_t > parseShortByteVector(
        const std::vector< uint8_t >& _tx, size_t& _offset, uint8_t _prefix );
    static inline std::vector< uint8_t > parseLongByteVector(
        const std::vector< uint8_t >& _tx, size_t& _offset, uint8_t _prefix );

    static inline size_t readLen( const std::vector< uint8_t >& _tx, size_t _offset, size_t _len );
    static inline bool isTypedTransaction( uint8_t prefix );
    static inline void skipRlpListHeader( const std::vector< uint8_t >& _tx, size_t& _offset );
    static inline std::vector< uint8_t > parseByteVector(
        const std::vector< uint8_t >& _tx, size_t& _offset );


public:
    static ptr< ParsedEthTransaction > parse( const std::vector< uint8_t >& _rawTx );
    ptr<vector< uint8_t >> getTransactionDataField();
    static void testEthereumTxParser();
    void validateToField();
    void validateFieldsCount() const;
};
