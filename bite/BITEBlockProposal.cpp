
#include "Log.h"
#include "BITEBlockProposal.h"

BITEBlockProposal::BITEBlockProposal( uint64_t timeStamp, uint32_t timeStampMs )
    : BlockProposal( timeStamp, timeStampMs ) {}

BITEBlockProposal::BITEBlockProposal( const schain_id& sChainId, const node_id& proposerNodeId,
    const block_id& blockId, const schain_index& proposerIndex,
    const ptr< TransactionList >& transactions, const u256& stateRoot, uint64_t timeStamp,
    __uint32_t timeStampMs, const string& signature, const ptr< CryptoManager >& cryptoManager )
    : BlockProposal( sChainId, proposerNodeId, blockId, proposerIndex, transactions, stateRoot,
          timeStamp, timeStampMs, signature, cryptoManager ) {}
