#pragma once

#include "datastructures/CommittedBlock.h"

class BITECommittedBlock : public CommittedBlock {

    friend class CommittedBlock;

protected:

    BITECommittedBlock( const schain_id& schainId, const node_id& proposerNodeId,
        const block_id& blockId, const schain_index& proposerIndex,
        const ptr< TransactionList >& transactions, const u256& stateRoot, uint64_t timeStamp,
        __uint32_t timeStampMs, const string& signature, const string& thresholdSig,
        const string& daSig );

    ptr< std::vector< uint8_t > > serializeTransactionsAndCompleteSerialization(
        ptr< BasicHeader > _blockHeader ) override;

    static ptr< CommittedBlock > deserialize( const ptr< vector< uint8_t > >& _serializedBlock,
        const ptr< CryptoManager >& _manager, bool _verifySig );

    static void serializedSanityCheck( const ptr< vector< uint8_t > >& _serializedBlock );
};
