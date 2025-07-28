#include "EthTransaction.h"
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/rand.h>
#include <openssl/sha.h>


#include <SkaleCommon.h>
#include "Log.h"
#include "node/ConsensusInterface.h"
#include "bite/BiteDataFiled.h"
#include "bite/BiteManager.h"
#include "libBLS/threshold_encryption/ThresholdEncryption.h"
#include "crypto/EncryptedAESKey.h"
#include "ParsedEthTransaction.h"
#include "EthTransactionEncoder.h"
#include "RLPStream.h"

#pragma GCC diagnostic pop


Signature::Signature(const uint256& _v, const uint256& _r, const uint256& _s) :
    v(RLPStream::bytesToU256(_v)),
    r(RLPStream::bytesToU256(_r)),
    s(RLPStream::bytesToU256(_s)) {}

// -----------------------------------------------------------------------------------
//                                  EthTransaction base
// -----------------------------------------------------------------------------------


/// --------------------------- RLP Encoding --------------------------- ///

std::vector< uint8_t > EthTransaction::rlpEncode(const std::optional< Signature > &signature) const {
    // get tx fields encoded in order as a vec of byte vectors
    RLPStream fields = encode();

    if ( signature ) {
        fields << signature->v
                << signature->r
                << signature->s;
    } else {
        u256 ZERO = 0;
        fields << ZERO // v
                << ZERO // r
                << ZERO; // s
    }

    std::vector< uint8_t > encoded = fields.encode();

    // insert tx prefix at beginning if any
    auto prefix = getBytePrefix();
    switch (prefix) {
        case TxPrefix::TYPE1:
        case TxPrefix::TYPE2:
            encoded.insert(encoded.begin(), static_cast<uint8_t>(prefix));
            break;
        default:
            break;
    }
    return encoded;
}

std::vector< uint8_t > AccessTuple::encode() const {
    // encode address
    RLPStream rlpAddress;
    rlpAddress << address;

    // encode each item inside list of storage keys
    RLPStream rlpKeys;
    for (const auto& key : storageKeys) {
        rlpKeys << key;
    }

    RLPStream rlpAccessTuple;
    rlpAccessTuple << rlpAddress.encode() // encode the address
                   << rlpKeys.encode(); // encode the list of storage keys

    return rlpAccessTuple.encode();
}


/// --------------------------- Signature --------------------------- ///

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"

auto EthTransaction::getHashContext() {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    CHECK_STATE( ctx );  // Assuming this is a macro or function to check nullptr
    return std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>( ctx, &EVP_MD_CTX_free);
}

EthTransaction::SecpCtxPtr EthTransaction::getSecp256k1SignContext() {
    secp256k1_context* raw = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    CHECK_STATE(raw);
    return std::unique_ptr<secp256k1_context, decltype(&secp256k1_context_destroy)>(
        raw, &secp256k1_context_destroy
    );
};

EthTransaction::SecpCtxPtr EthTransaction::getSecp256k1VerifyContext() {
    secp256k1_context* raw = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
    CHECK_STATE(raw);
    return std::unique_ptr<secp256k1_context, decltype(&secp256k1_context_destroy)>(
        raw, &secp256k1_context_destroy
    );
};

#pragma GCC diagnostic pop

std::array< uint8_t, 32 > EthTransaction::hash() const {
    std::vector< uint8_t > encoded_tx = rlpEncode( std::nullopt );

    std::array< uint8_t, 32 > hash;

    thread_local auto ctx = getHashContext();
    EVP_DigestInit_ex( ctx.get(), EVP_sha3_256(), NULL );
    EVP_DigestUpdate( ctx.get(), data.data(), data.size() );
    EVP_DigestFinal_ex( ctx.get(), hash.data(), NULL );
    return hash;
}

Signature EthTransaction::sign(std::vector< uint8_t > privateKey) const {
    
    auto txHash = hash();

    // thread local because it is not thread safe, but can be reusef
    auto ctx = getSecp256k1SignContext();

    // Sign hash using recoverable signature
    secp256k1_ecdsa_recoverable_signature signature;
    CHECK_STATE2(secp256k1_ecdsa_sign_recoverable(
        ctx.get(), &signature, txHash.data(), privateKey.data(), nullptr, nullptr ), 
        "Failed to sign transaction" );

    // Extract r, s, recovery ID
    uint8_t sig64[64];
    int rec_id = 0;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(
        ctx.get(), sig64, &rec_id, &signature );

    std::vector< uint8_t > r_bytes( sig64, sig64 + 32 );
    std::vector< uint8_t > s_bytes( sig64 + 32, sig64 + 64 );

    return Signature {
        computeSignatureV(rec_id),
        RLPStream::bytesToU256(r_bytes),
        RLPStream::bytesToU256(s_bytes)
    };
}

void EthTransaction::validateSignatureDomain(const Signature &sig) {
    static const u256 s_max{ "0xfffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141" };
    static const u256 s_zero;

    CHECK_STATE2(sig.v <= 1, "Expected v <= 1, got: " + sig.v.str());
    CHECK_STATE2(sig.r > s_zero, "Expected r > 0, got: " + sig.r.str());
    CHECK_STATE2(sig.s > s_zero, "Expected s > 0, got: " + sig.s.str());
    CHECK_STATE2(sig.r < s_max, "Expected r < s_max, got: " + sig.r.str());
    CHECK_STATE2(sig.s < s_max, "Expected s < s_max, got: " + sig.s.str());
}

void EthTransaction::verifySignature(Signature &sig) const {

    auto txHash = hash();


    if (verifiedTransactionHashes.exists(Key20(txHash))) {
        // we already verified signature for this transaction hash
        return;
    }


    sig.v = recoverSignatureV(sig);

    validateSignatureDomain(sig);

    // Create recoverable signature
    std::vector< uint8_t > r = RLPStream::u256toBytes(sig.r);
    std::vector< uint8_t > s = RLPStream::u256toBytes(sig.s);
    uint8_t sig64[64];
    std::copy(r.begin(), r.end(), sig64);
    std::copy(s.begin(), s.end(), sig64 + 32);

    // Static secp256k1 context for verification. A single contect for each thread
    // this is because context is not thread safe
    thread_local auto ctx = getSecp256k1VerifyContext();

    // Parse raw s, r, v into a signature struct
    // at this point v should hold its value as 1 byte.
    // In case of Legacy txs, v is replaced by the recovery ID (see above)
    int v_int = static_cast<int>(sig.v);
    secp256k1_ecdsa_recoverable_signature signature;
    CHECK_STATE2(secp256k1_ecdsa_recoverable_signature_parse_compact(ctx.get(), &signature, sig64, v_int),
        "Failed to parse recoverable signature");
    
    // Recover public key from signature
    secp256k1_pubkey pubkey;
    CHECK_STATE2(secp256k1_ecdsa_recover(ctx.get(), &pubkey, &signature, txHash.data()),
        "Failed to recover public key from signature");

    verifiedTransactionHashes.putIfDoesNotExist(Key20(txHash), true);
}

// -----------------------------------------------------------------------------------
//                                      Legacy
// -----------------------------------------------------------------------------------

RLPStream LegacyTx::encode() const {
    RLPStream fields;
    fields << nonce
           << gasPrice
           << gasLimit
           << to
           << value
           << data;

    return fields;
}

TxPrefix LegacyTx::getBytePrefix() const {
    return TxPrefix::NONE;
}

u256 LegacyTx::recoverSignatureV(const Signature &sig) const {
    if (!sig.r && !sig.s) {
        return 0;
    }
    else {
        CHECK_STATE2(sig.v > 36 || ( sig.v == 27 || sig.v == 28 ),
        "Invalid signature. Expected <= 36 or 27 or 28, got: " + sig.v.str());

        u256 recoveryId;
        if ( sig.v == 27 || sig.v == 28 )
            recoveryId = sig.v - 27;
        else {
            u256 const chain = ( sig.v - 35 ) / 2;
            CHECK_STATE2( chain <= std::numeric_limits< uint64_t >::max(),
            "Invalid signature: Legacy Tx chainID overflow" );
            recoveryId = sig.v - ( chain * 2 + 35 );
        }  
        return recoveryId;
    }
}

u256 LegacyTx::computeSignatureV(int rec_id) const {
    // Compute correct `v` as per EIP-155
    uint64_t chain_id = 0;
    for ( uint8_t byte : this->chainId ) {
        chain_id = ( chain_id << 8 ) | byte;
    }

    return static_cast<u256>(chain_id * 2 + 35 + rec_id);
}

// -----------------------------------------------------------------------------------
//                                      Type 1
// -----------------------------------------------------------------------------------

RLPStream Type1Tx::encode() const {
    RLPStream fields;
    fields << chainId
            << nonce
            << gasPrice
            << gasLimit
            << to
            << value
            << data;

    // encode access list - new field for EIP-2930
    RLPStream rlpAccessList;
    for ( const auto& accessTuple : accessList ) {
        std::vector< uint8_t > rlpAccessTuple = accessTuple.encode();
        rlpAccessList << rlpAccessTuple;
    }

    fields << rlpAccessList.encode();

    return fields;
}

TxPrefix Type1Tx::getBytePrefix() const {
    return TxPrefix::TYPE1;
}

u256 Type1Tx::recoverSignatureV(const Signature &sig) const {
    return sig.v;
}

u256 Type1Tx::computeSignatureV(int rec_id) const {
    return static_cast<u256>(rec_id);
}

// -----------------------------------------------------------------------------------
//                                      Type 2
// -----------------------------------------------------------------------------------

RLPStream Type2Tx::encode() const {
    RLPStream fields;
    fields << chainId
            << nonce
            << maxPriorityFeePerGas   // new field for EIP-1559
            << maxFeePerGas           // new field for EIP-1559
            << gasLimit
            << to
            << value
            << data;

    RLPStream rlpAccessList;
    for ( const auto& accessTuple : accessList ) {
        std::vector< uint8_t > rlpAccessTuple = accessTuple.encode();
        rlpAccessList << rlpAccessTuple;
    }

    fields << rlpAccessList.encode();

    return fields;
}

TxPrefix Type2Tx::getBytePrefix() const {
    return TxPrefix::TYPE2;
}

u256 Type2Tx::recoverSignatureV(const Signature &sig) const {
    return sig.v;
}

u256 Type2Tx::computeSignatureV(int rec_id) const {
    return static_cast<u256>(rec_id);
}

cache::lru_cache<Key20, bool> EthTransaction::verifiedTransactionHashes(VERIFIED_TX_SIGS_CACHE_SIZE);