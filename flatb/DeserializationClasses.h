//
// Created by stan on 14-03-2025.
//

#ifndef SKALED_DESERIALIZATIONCLASSES_H
#define SKALED_DESERIALIZATIONCLASSES_H



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








}
#endif  // SKALED_DESERIALIZATIONCLASSES_H
