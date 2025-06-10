#pragma once

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <string>

class ParsedEthTransaction {
    uint8_t type = 0;  // 0 = legacy
    std::vector< std::vector< uint8_t > > fields;


    inline void parseTransactionFields( const std::vector< uint8_t >& _tx, size_t& _offset, int fieldCount);

public:
    const std::vector< std::vector< uint8_t > >& getFields() const;
    uint8_t getType() const;

private:
    inline void validateAll();
    inline void validateSignature();
    inline bool isZero(const std::vector<uint8_t>& _data );

    static inline std::vector< uint8_t > parseSingleByteVector(
        const std::vector< uint8_t >& _tx, size_t& _offset );
    static inline std::vector< uint8_t > parseShortByteVector(
        const std::vector< uint8_t >& _tx, size_t& _offset, uint8_t _prefix );
    static inline std::vector< uint8_t > parseLongByteVector(
        const std::vector< uint8_t >& _tx, size_t& _offset, uint8_t _prefix );

    /// The 2 methods below do not return the actual decoded value representing the lists,
    /// but actually only the last decoded value within that list (or list of lists, and so on).
    /// This is because we only care about the structure, not the content. These methods are used to
    /// parse the RLP structure.
    static inline std::vector<uint8_t> parseShortList(
        const std::vector<uint8_t>& tx, uint64_t& offset, uint8_t prefix );
    static inline std::vector<uint8_t> parseLongList(
        const std::vector<uint8_t>& tx, uint64_t& offset, uint8_t prefix);

    static inline size_t readLen( const std::vector< uint8_t >& _tx, size_t _offset, size_t _len );
    static inline bool isTypedTransaction( uint8_t prefix );
    static inline void skipRlpListHeader( const std::vector< uint8_t >& _tx, size_t& _offset );
    static inline std::vector< uint8_t > parseBytes(
        const std::vector< uint8_t >& _tx, size_t& _offset );

    size_t getToFieldIndex() const;
    size_t getDataFieldIndex() const;


public:
    static ptr< ParsedEthTransaction > parse( const std::vector< uint8_t >& _rawTx );
    // TODO should remove the 'Transaction' from the name
    ptr<vector< uint8_t >> getTransactionDataField();
    ptr<vector< uint8_t >> getToField() const;
    bool hasToField() const;
    static void testEthereumTxParser();
    void validateToField();
    void validateFieldsCount() const;
    void setTransactionDataField( vector< uint8_t >& _dataField );
};
