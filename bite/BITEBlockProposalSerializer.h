#pragma once


#include "datastructures/BlockProposal.h"


class BITEBlockProposalSerializer {

public:

    static ptr< vector< uint8_t > > serializeTransactionsAndCompleteSerialization(
        ptr< BasicHeader > _blockHeader, ptr<TransactionList> _transactionList );

    static ptr< BlockProposal > deserialize( const ptr< vector< uint8_t > >& _serializedProposal,
        const ptr< CryptoManager >& _manager, bool _verifySig );

    static void serializedSanityCheck( const ptr< vector< uint8_t > >& _serializedBlock );

};