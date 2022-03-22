//
// Created by skale on 22.03.22.
//
#include "SkaleCommon.h"
#include "BlockVerifier.h"


void BlockVerifier::verifyThresholdSig(
        ptr< ThresholdSignature > , BLAKE3Hash& ,
        const TimeStamp& ) {
    return;
};

void  BlockVerifier::verifyBlockSig(ptr< ThresholdSignature > ,  BLAKE3Hash & , const TimeStamp&  ) {
    return;
};

void  BlockVerifier::verifyBlockSig(string& ,  block_id , BLAKE3Hash & , const TimeStamp&  ) {
    return;
}

bool BlockVerifier::verifyProposalECDSA(
        const ptr< BlockProposal >& , const string& , const string&  ) {
    return true;
};
