#pragma once


#include "Agent.h"

class ConsensusSigShareSet;
class ConsensusBLSSignature;
class Schain;
class ConsensusBLSSigShare;
class ThresholdSigShareSet;
class ThresholdSignature;
class ThresholdSigShare;
class BooleanProposalVector;
class DAProof;
class BlockProposal;

#include "CacheLevelDB.h"


class TEDecryptionDB : public CacheLevelDB {
    recursive_mutex daProofMutex;

public:
    explicit TEDecryptionDB(
        Schain* _sChain, string& _dirName, string& _prefix, node_id _nodeId, uint64_t _maxDBSize );

    ptr< BooleanProposalVector > addDAProof( const ptr< DAProof >& _daProof );

    const string& getFormatVersion();

    bool haveDAProof( const ptr< BlockProposal >& _proposal );

    bool isEnoughProofs( block_id _blockID );

    string getDASig( block_id _blockId, schain_index _proposerIndex );
};
