#pragma once
#include <vector>
#include <cstdint>
#include "RLPStream.h"
#include "RLP.h"

// Suppress deprecated-copy warning caused by Boost.Multiprecision (cpp_int)
// Boost defines a user-provided copy constructor but no copy assignment operator,
// which causes GCC >=11 to emit -Wdeprecated-copy during move assignment.
// This pragma silences the warning locally for Boost headers.
// Assignments of the struct `Signature` might fall back to copying, 
// even though it looks like you're doing a move
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"

#include <boost/multiprecision/cpp_int.hpp>

#pragma GCC diagnostic pop

#include <optional>
#include <secp256k1.h>
#include <secp256k1_recovery.h>


enum class TxType {
    LEGACY = 0,
    TYPE1 = 1,
    TYPE2 = 2
};

enum class TxPrefix : int8_t {
    TYPE1 = 0x01,
    TYPE2 = 0x02,
    NONE = 0x00,
};

struct Signature {
    u256 v;
    u256 r;
    u256 s;

    Signature(const uint256& _v, const uint256& _r, const uint256& _s);

    Signature(const u256& _v, const u256& _r, const u256& _s) 
    : v(_v), r(_r), s(_s) {}

    Signature() = default;
    Signature(const Signature&) = default;
    Signature(Signature&&) noexcept = default;
    Signature& operator=(const Signature&) = default;
    Signature& operator=(Signature&&) noexcept = default;
    ~Signature() = default;
};

struct AccessTuple;

/**
 * @brief Base EthTransaction fields - common to all EthTransactions.
 * Fields defined in this struct do not follow RLP-encoded order.
 * This order should be enforced in the `encode` implementation for each
 * type of EthTransaction as well as the constructor of each type's class
 */
struct EthTransaction {
    uint256 nonce;
    uint256 gasLimit;
    std::vector< uint8_t > to;  // 20 bytes
    uint256 value;
    std::vector< uint8_t > data;
    // not included in RLP-encoding for legacy tx
    uint256 chainId;

    EthTransaction(
        const uint256& nonce,
        const uint256& gasLimit,
        const std::vector<uint8_t>& to,
        const uint256& value,
        const std::vector<uint8_t>& data,
        const uint256& chainId)
        : nonce(nonce), gasLimit(gasLimit), to(to), value(value), data(data), chainId(chainId)
    {}
    
    /**
     * @brief Signs a transaction, returning the resulting signature.
     */
    Signature sign(std::vector< uint8_t > privateKey) const;

    /**
     * @brief Encodes the EthTransaction into RLP format.
     */
    std::vector< uint8_t > rlpEncode(const std::optional< Signature > &signature) const;

    /**
     * @brief Verifies the signature of the transaction.
     * Replaces the v field with recovered v value depending on tx type
     */
    void verifySignature(Signature &signature) const;

    /**
     * @brief Hashes the transaction using Keccak-256.
     * Converts the transaction to RLP format, and then hashes it.
     */
    std::array< uint8_t, 32 > hash() const;

    /**
     * @brief Returns a valid prefix representing the tx type.
     * Returns -1 if the EthTransaction type does not include prefix
     */
    virtual TxPrefix getBytePrefix() const = 0;

protected:
        
    //  -------------------- Helper functions for transaction signing ---------------------------//

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wignored-attributes"

    using SecpCtxPtr = std::unique_ptr< secp256k1_context, decltype(&secp256k1_context_destroy) >;

    #pragma GCC diagnostic pop

    // a single context of each type can be used till the end of the program
    static SecpCtxPtr getSecp256k1VerifyContext();
    static SecpCtxPtr getSecp256k1SignContext();
    static auto getHashContext();

    static void validateSignatureDomain(const Signature& signature);
    
    //  -------------------------- Type-specific functionality ----------------------------------//
    /**
     * @brief Encode the object into RLP format.
     * Field order is enforced by this function. Does not include signature fields in the rlp encoding.
     * Used by the `Transacition::rlpEncode` function.
     */
    virtual RLPStream encode() const = 0;

    /**
     * @brief Signs the transaction using type-specific logic of either LegacyTx, Type1Tx or Type2Tx.
     */
    virtual u256 computeSignatureV(int rec_id) const = 0;

    virtual u256 recoverSignatureV(const Signature& sig) const = 0;
    
    // Allow accessing the rlpEncoding functions from Transaction
    friend struct AccessTuple;
    // Allow accessing Secp256k1SignContext for generating private key
    friend class EthTransactionEncoder;
    // Allow access to bytes to u256 conversion functions
    friend struct Signature;
};

struct LegacyTx : EthTransaction {
    uint256 gasPrice;

    LegacyTx(
        const uint256& nonce,
        const uint256& gasLimit,
        const std::vector<uint8_t>& to,
        const uint256& value,
        const std::vector<uint8_t>& data,
        const uint256& chainId,
        const uint256& _gasPrice)
    : EthTransaction(nonce, gasLimit, to, value, data, chainId),
    gasPrice(_gasPrice)
    {}

    LegacyTx(RLPItem& fields) :
        EthTransaction(
            fields[ 0 ].asBytes(), // nonce
            fields[ 2 ].asBytes(), // gasLimit
            fields[ 3 ].asBytes(), // to
            fields[ 4 ].asBytes(), // value
            fields[ 5 ].asBytes(), // data
            {} // chainId
        ),
        gasPrice(fields[ 1 ].asBytes())
    {}

    RLPStream encode() const override;
    u256 computeSignatureV(int rec_id) const override;
    u256 recoverSignatureV(const Signature& sig) const override;
    TxPrefix getBytePrefix() const override;
};

/**
 * @brief Access tuple for EIP-2930 EthTransactions
 */
struct AccessTuple {
    std::vector<uint8_t> address;                  // 20 bytes
    std::vector<std::vector<uint8_t>> storageKeys; // 32-byte keys

    std::vector< uint8_t > encode() const;
};

/**
 *  @brief EIP-2930 EthTransaction fields - does not need to follow the same
 *  order - the order will be enforced in the `encode` method
 */
struct Type1Tx : EthTransaction {
    uint256 gasPrice;
    std::vector<AccessTuple> accessList;

    Type1Tx(
        const uint256& nonce,
        const uint256& gasLimit,
        const std::vector<uint8_t>& to,
        const uint256& value,
        const std::vector<uint8_t>& data,
        const uint256& chainId,
        const uint256& _gasPrice,
        const std::vector<AccessTuple>& _accessList)
        : EthTransaction(nonce, gasLimit, to, value, data, chainId),
        gasPrice(_gasPrice), accessList(_accessList)
    {}

    Type1Tx(RLPItem& fields) :
        EthTransaction(
            fields[ 1 ].asBytes(), // nonce
            fields[ 3 ].asBytes(), // gasLimit
            fields[ 4 ].asBytes(), // to
            fields[ 5 ].asBytes(), // value
            fields[ 6 ].asBytes(), // data
            fields[ 0 ].asBytes()  // chainId
        ),
        gasPrice(fields[ 2 ].asBytes()),
        accessList({})
    {}

    RLPStream encode() const override;
    u256 computeSignatureV(int rec_id) const override;
    u256 recoverSignatureV(const Signature& sig) const override;
    TxPrefix getBytePrefix() const override;
};

/**
 *  @brief EIP-1559 EthTransaction fields - does not need to follow the same
 *  order - the order will be enforced in the `encode` method
 */
struct Type2Tx : EthTransaction {
    uint256 maxPriorityFeePerGas;
    uint256 maxFeePerGas;
    std::vector<AccessTuple> accessList;

    Type2Tx(
        const uint256& nonce,
        const uint256& gasLimit,
        const std::vector<uint8_t>& to,
        const uint256& value,
        const std::vector<uint8_t>& data,
        const uint256& chainId,
        const uint256& _maxPriorityFeePerGas,
        const uint256& _maxFeePerGas,
        const std::vector<AccessTuple>& _accessList)
        : EthTransaction(nonce, gasLimit, to, value, data, chainId),
        maxPriorityFeePerGas(_maxPriorityFeePerGas), maxFeePerGas(_maxFeePerGas), accessList(_accessList)
    {}

    Type2Tx(RLPItem& fields) :
    EthTransaction(
        fields[ 1 ].asBytes(), // nonce
        fields[ 4 ].asBytes(), // gasLimit
        fields[ 5 ].asBytes(), // to
        fields[ 6 ].asBytes(), // value
        fields[ 7 ].asBytes(), // data
        fields[ 0 ].asBytes() // chainId
        ),
        maxPriorityFeePerGas(fields[ 2 ].asBytes()),
        maxFeePerGas(fields[ 3 ].asBytes()),
        accessList({})
    {}

    RLPStream encode() const override;
    u256 computeSignatureV(int rec_id) const override;
    u256 recoverSignatureV(const Signature& sig) const override;
    TxPrefix getBytePrefix() const override;
};