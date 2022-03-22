//
// Created by skale on 22.03.22.
//

#ifndef SKALED_BLOCKVERIFIER_H
#define SKALED_BLOCKVERIFIER_H

#include "CryptoVerifier.h"

class BlockVerifier :  public CryptoVerifier{

    void verifyThresholdSig(
            ptr< ThresholdSignature > _signature, BLAKE3Hash& _hash,
                     const TimeStamp& _ts = TimeStamp(uint64_t(-1), 0)) override;

    void  verifyBlockSig(ptr< ThresholdSignature > _signature,  BLAKE3Hash & _hash,
                         const TimeStamp& _ts = TimeStamp(uint64_t(-1), 0)) override;

    void  verifyBlockSig(string& _signature,  block_id _blockId, BLAKE3Hash & _hash,
                         const TimeStamp& _ts = TimeStamp(uint64_t(-1), 0)) override;

    bool verifyProposalECDSA(
            const ptr< BlockProposal >& _proposal, const string& _hashStr, const string& _signature ) override;


};


#endif //SKALED_BLOCKVERIFIER_H
