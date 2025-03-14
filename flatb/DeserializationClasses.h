//
// Created by stan on 14-03-2025.
//

#ifndef SKALED_DESERIALIZATIONCLASSES_H
#define SKALED_DESERIALIZATIONCLASSES_H

#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <folly/io/IOBuf.h>
#include "block_finalize_request_generated.h"  // Include FlatBuffers-generated headers

namespace block_finalize {

// Transaction (Byte Array)
class Transaction {
public:
    std::vector< uint8_t > data;
};


// Hash (Fixed-size 32-byte array)
class Hash {
public:
    std::array< uint8_t, 32 > data;
};


// TruncatedHash (Fixed-size 20-byte array)
class TruncatedHash {
public:
    std::array< uint8_t, 20 > data;
};

// BlockHeader (Fixed structure)
class BlockHeader {
public:
    uint64_t schainId;
    uint64_t epochId;
    uint64_t blockId;
    uint64_t proposerIndex;
    uint64_t transactionCount;
    uint64_t timestampS;
    uint64_t timestampMs;
    Hash transactionsMerkleRoot;
    Hash parentHash;
    std::array< uint8_t, 32 > extraData;
};

// BlockFragment (Flattened)
class BlockFragment {
public:
    uint64_t index;
    std::vector< TruncatedHash > txTruncatedHashes;
    std::vector< Hash > leftProof;
    std::vector< Hash > rightProof;
    BlockFragment() = default;
};

// DecryptionShare
class DecryptionShare {
public:
    uint64_t transactionIndex;
    std::vector< uint8_t > data;
    DecryptionShare() = default;
};

// ErrorResponse
class ErrorResponse {
public:
    uint32_t status;
    uint32_t substatus;
    std::string message;
    ErrorResponse() = default;
};

// BlockFinalizeRequest
class BlockFinalizeRequestObject {
public:
    BlockFinalizeRequestObject( uint64_t schainId, uint64_t epochId, uint64_t blockId,
        uint64_t nodeId, uint64_t proposerIndex, bool needDaProofSig, bool needDecryptionShares,
        bool needFragment, uint64_t fragmentIndex )
        : schainId( schainId ),
          epochId( epochId ),
          blockId( blockId ),
          nodeId( nodeId ),
          proposerIndex( proposerIndex ),
          needDaProofSig( needDaProofSig ),
          needDecryptionShares( needDecryptionShares ),
          needFragment( needFragment ),
          fragmentIndex( fragmentIndex ) {}

private:
    uint64_t schainId;
    uint64_t epochId;
    uint64_t blockId;
    uint64_t nodeId;
    uint64_t proposerIndex;
    bool needDaProofSig;
    bool needDecryptionShares;
    bool needFragment;
    uint64_t fragmentIndex;
};



BlockFinalizeRequestObject deserializeBlockFinalizeRequest(const folly::IOBuf& _buffer) {


    flatbuffers::Verifier verifier(_buffer.data(), _buffer.length());
    if (!block_finalize::VerifyBlockFinalizeRequestBuffer(verifier)) {
        std::cerr << "Error: FlatBuffer verification failed." << std::endl;
        throw std::runtime_error("Invalid FlatBuffer data");
    }

    auto request =
        flatbuffers::GetRoot<block_finalize::BlockFinalizeRequest>(_buffer.data());

    return { request->schain_id(), request->epoch_id(), request->block_id(), request->node_id(),
        request->proposer_index(), request->need_da_proof_sig(), request->need_decryption_shares(),
        request->need_fragment(), request->fragment_index() };

}

}
#endif  // SKALED_DESERIALIZATIONCLASSES_H
