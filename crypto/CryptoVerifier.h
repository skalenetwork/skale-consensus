//
// Created by skale on 22.03.22.
//

#ifndef SKALED_CRYPTOVERIFIER_H
#define SKALED_CRYPTOVERIFIER_H

#include "datastructures/TimeStamp.h"

class ThresholdSignature;
class BLAKE3Hash;
class BlockProposal;

class CryptoVerifier {

public:

    virtual void verifyThresholdSig(
            ptr< ThresholdSignature > _signature, BLAKE3Hash& _hash, bool _forceMockup, const TimeStamp& _ts = TimeStamp(uint64_t(-1), 0)) = 0;

    virtual void  verifyBlockSig(ptr< ThresholdSignature > _signature,  BLAKE3Hash & _hash, const TimeStamp& _ts = TimeStamp(uint64_t(-1), 0)) = 0;

    virtual void  verifyBlockSig(string& _signature,  block_id _blockId, BLAKE3Hash & _hash, const TimeStamp& _ts = TimeStamp(uint64_t(-1), 0)) = 0;

    virtual bool verifyProposalECDSA(
            const ptr< BlockProposal >& _proposal, const string& _hashStr, const string& _signature ) = 0;

};


#endif //SKALED_CRYPTOVERIFIER_H
