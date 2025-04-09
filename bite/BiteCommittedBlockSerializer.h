#pragma once

class DecryptedAESKeyList;

#include "datastructures/CommittedBlock.h"

class BiteCommittedBlockSerializer {

public:

    static ptr< std::vector< uint8_t > > serializeTransactionsAndCompleteSerialization(
        ptr< BasicHeader > _blockHeader, ptr<TransactionList> _transactionList, ptr<DecryptedAESKeyList> _aesKeyList, schain_index
        _proposerIndex);

    static ptr< CommittedBlock > deserialize( const ptr< vector< uint8_t > >& _serializedBlock,
        const ptr< CryptoManager >& _manager, bool _verifySig );

    static void serializedSanityCheck( const ptr< vector< uint8_t > >& _serializedBlock );
};
