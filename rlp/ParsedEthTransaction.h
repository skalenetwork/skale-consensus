#pragma once

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <string>


#include "RLP.h"





class ParsedEthTransaction {
    uint8_t type = 0;  // 0 = legacy
    RLPItem fields;

    inline void parseTransactionFields( const std::vector< uint8_t >& _tx, size_t& _offset, int fieldCount);

public:
    const RLPItem& getFields() const;
    uint8_t getType() const;

private:
    inline void validateAll() const;
    inline void validateSignature() const;
    inline bool isZero(const std::vector<uint8_t>& _data ) const;
    static inline bool isTypedTransaction( uint8_t prefix );

    size_t getToFieldIndex() const;
    size_t getDataFieldIndex() const;

public:
    static ptr< ParsedEthTransaction > parse( const std::vector< uint8_t >& _rawTx );
    // TODO should remove the 'Transaction' from the name
    ptr<vector< uint8_t >> getTransactionDataField();
    ptr<vector< uint8_t >> getToField() const;
    bool hasToField() const;
    static void testEthereumTxParser();
    void validateToField() const;
    void validateFieldsCount() const;
    void setTransactionDataField( vector< uint8_t >& _dataField );
};
