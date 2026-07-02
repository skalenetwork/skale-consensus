#pragma once

class DecryptedAESKeyList;

#include "datastructures/CommittedBlock.h"

class BiteCommittedBlockSerializer {

public:

    static ptr< std::vector< uint8_t > > serializeTransactionsAndCompleteSerialization(
        BasicHeader& _blockHeader, const TransactionList& _transactionList, const DecryptedAESKeyList& _aesKeyList);

    static ptr< CommittedBlock > deserialize(const ptr< vector< uint8_t > >& _serializedBlock,
                                             const ptr< CryptoManager >& _cryptoManager, ptr<BiteManager> _biteManager, bool _verifySig);

    static void serializedSanityCheck( const ptr< vector< uint8_t > >& _serializedBlock );
};
