#pragma once
#include <optional>
#include <memory>
#include "EthTransaction.h"
#include "SkaleCommon.h"

class ParsedEthTransaction;
class BiteManager;


class EthTransactionEncoder {

public:
    static std::shared_ptr<std::vector<uint8_t>> generateSampleTx(bool _isByte, ptr<BiteManager> _biteManager);


    /// Validates a signature against the hash of a message structurally.
    /// It does not validate the signature by checking if such account exists
    /// using the recovered public key from the signature.
    /// Meaning, a malicious transaction (with tampered hash, but structurally correct)
    /// can still pass though this check.
    static void verifyEthSignature( const std::vector< uint8_t >& v_vec,
        const std::vector< uint8_t >& r_bytes, const std::vector< uint8_t >& s_bytes,
        const std::vector< uint8_t >& tx_hash, const TxType& type);


    // static std::vector< uint8_t > rlpEncode( const EthTransaction& tx, bool withSig,
    //     std::vector< uint8_t >* v_encoded, std::vector< uint8_t >* r_encoded,
    //     std::vector< uint8_t >* s_encoded );

    static std::shared_ptr< std::vector< uint8_t > >  rlpEncodeWithoutSig(ParsedEthTransaction& _transaction);


    static std::vector< uint8_t > hashTransaction( const std::vector< uint8_t >& tx );

public:


    // a single context of each type can be used till the end of the program
    inline static auto getSecp256k1VerifyContext();
    inline static auto getSecp256k1SignContext();
    inline static auto getHashContext();

    /// Bytes size must be at least 32
    /// Assumes bytes are big endian
    /// MSB on the left (bytes[0]), least significant byte on the right (bytes[31])
    inline static u256 bytesToU256( const std::vector< uint8_t >& bytes );

    inline static void validateSignature( const u256& r, const u256& s, const u256& v);


    inline static std::vector< uint8_t > generateRandomPrivateKey();

    inline static std::vector< uint8_t > keccak256( const std::vector< uint8_t >& data );

    static std::shared_ptr<std::vector<uint8_t>> signAndEncodeTx(const EthTransaction& tx);


};
