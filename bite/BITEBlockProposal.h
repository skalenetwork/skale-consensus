#pragma once


#include "datastructures/BlockProposal.h"


class BITEBlockProposal : public BlockProposal {
public:
    BITEBlockProposal( uint64_t timeStamp, uint32_t timeStampMs );
    BITEBlockProposal( const schain_id& sChainId, const node_id& proposerNodeId,
        const block_id& blockId, const schain_index& proposerIndex,
        const ptr< TransactionList >& transactions, const u256& stateRoot, uint64_t timeStamp,
        __uint32_t timeStampMs, const string& signature,
        const ptr< CryptoManager >& cryptoManager );
};
