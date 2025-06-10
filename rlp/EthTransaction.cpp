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

#pragma GCC diagnostic pop


Signature::Signature(const uint256& _v, const uint256& _r, const uint256& _s) :
    v(EthTransaction::bytesToU256(_v)),
    r(EthTransaction::bytesToU256(_r)),
    s(EthTransaction::bytesToU256(_s)) {}

// -----------------------------------------------------------------------------------
//                                  EthTransaction base
// -----------------------------------------------------------------------------------


/// --------------------------- RLP Encoding --------------------------- ///

void EthTransaction::addEncodedFieldUint256(std::vector< std::vector< uint8_t > >& fields, const uint256& val) {
    std::vector< uint8_t > tmp;
    rlpEncodeUint256(tmp, val);
    fields.push_back(tmp);
}

void EthTransaction::addEncodedFieldBytes(std::vector< std::vector< uint8_t > >& fields, const std::vector< uint8_t >& val) {
    std::vector< uint8_t > tmp;
    rlpEncodeBytes(tmp, val);
    fields.push_back(tmp);
}

void EthTransaction::addEncodedFieldList(std::vector< std::vector< uint8_t > >& fields, const std::vector< std::vector< uint8_t > >& val) {
    std::vector< uint8_t > tmp;
    rlpEncodeList(tmp, val);
    fields.push_back(tmp);
}

void EthTransaction::rlpEncodeBytes(
    std::vector< uint8_t >& out, const std::vector< uint8_t >& data ) {
    const size_t len = data.size();

    // Explicitly handle empty string
    if ( len == 0 ) {
        out.push_back( 0x80 );
        return;
    }

    // Single byte < 0x80 is encoded directly (no prefix)
    if ( len == 1 && data[0] < 0x80 ) {
        out.push_back( data[0] );
        return;
    }

    if ( len < 56 ) {
        // Short string: prefix = 0x80 + len
        out.push_back( static_cast< uint8_t >( 0x80 + len ) );
    } else {
        // Long string: prefix = 0xb7 + len-of-len, then len, then data
        std::vector< uint8_t > len_bytes;
        size_t sz = len;
        while ( sz > 0 ) {
            len_bytes.insert( len_bytes.begin(), static_cast< uint8_t >( sz & 0xFF ) );
            sz >>= 8;
        }

        CHECK_STATE2( len_bytes.size() <= 8, "rlpEncodeBytes: data too large (exceeds 2^64-1)" );

        out.push_back( static_cast< uint8_t >( 0xb7 + len_bytes.size() ) );
        out.insert( out.end(), len_bytes.begin(), len_bytes.end() );
    }

    // Append actual data
    out.insert( out.end(), data.begin(), data.end() );
}


void EthTransaction::rlpEncodeUint256(
    std::vector< uint8_t >& out, const std::vector< uint8_t >& value ) {
    // Skip leading zeros
    size_t start = 0;
    while ( start < value.size() && value[start] == 0 ) {
        ++start;
    }

    // Extract minimal non-zero representation (or empty)
    std::vector< uint8_t > stripped( value.begin() + start, value.end() );

    // Delegate to rlp_encode_bytes
    rlpEncodeBytes( out, stripped );
}

void EthTransaction::rlpEncodeList(
    std::vector< uint8_t >& out, const std::vector< std::vector< uint8_t > >& elements ) {
    std::vector< uint8_t > payload;
    for ( const auto& e : elements ) {
        payload.insert( payload.end(), e.begin(), e.end() );
    }
    if ( payload.size() < 56 ) {
        out.push_back( 0xc0 + payload.size() );
    } else {
        std::vector< uint8_t > len;
        size_t sz = payload.size();
        while ( sz ) {
            len.insert( len.begin(), static_cast< uint8_t >( sz & 0xFF ) );
            sz >>= 8;
        }
        out.push_back( 0xf7 + len.size() );
        out.insert( out.end(), len.begin(), len.end() );
    }
    out.insert( out.end(), payload.begin(), payload.end() );
}

std::vector< uint8_t > EthTransaction::rlpEncode(const std::optional< Signature > &signature) const {
    // get tx fields encoded in order as a vec of byte vectors
    std::vector< std::vector< uint8_t > > fields = encode();

    if ( signature ) {
        std::vector< uint8_t > v_encoded, r_encoded, s_encoded;
        rlpEncodeUint256( v_encoded, u256toBytes(signature->v) );
        rlpEncodeUint256( r_encoded, u256toBytes(signature->r) );
        rlpEncodeUint256( s_encoded, u256toBytes(signature->s) );

        fields.push_back( v_encoded );
        fields.push_back( r_encoded );
        fields.push_back( s_encoded );
    } else {
        std::vector< uint8_t > tmp;
        std::vector< uint8_t > ZERO;
        rlpEncodeUint256( tmp, ZERO );
        fields.push_back( tmp );
        tmp.clear();
        rlpEncodeUint256( tmp, ZERO );
        fields.push_back( tmp );
        tmp.clear();
        rlpEncodeUint256( tmp, ZERO );
        fields.push_back( tmp );
        tmp.clear();
    }

    std::vector< uint8_t > encoded;
    rlpEncodeList( encoded, fields );

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
    std::vector< uint8_t > rlpAddress;
    EthTransaction::rlpEncodeBytes( rlpAddress, address );

    // encode each item inside list of storage keys
    std::vector<std::vector<uint8_t>> rlpKeys;
    for (const auto& key : storageKeys) {
        std::vector<uint8_t> rlpKey;
        EthTransaction::rlpEncodeBytes(rlpKey, key);
        rlpKeys.push_back(rlpKey);
    }

    // encode list of storage keys
    std::vector<uint8_t> rlpStorageKeys;
    EthTransaction::rlpEncodeList(rlpStorageKeys, rlpKeys);

    // create new list with encoded address and encoded list of storage keys
    const std::vector< std::vector< uint8_t > > rlpStorageKeysEncoded = {
        rlpAddress,
        rlpStorageKeys
    };

    // encode the final access tuple
    std::vector< uint8_t > rlpAccessTuple;
    EthTransaction::rlpEncodeList(rlpAccessTuple, rlpStorageKeysEncoded);

    return rlpAccessTuple;
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
        bytesToU256(r_bytes),
        bytesToU256(s_bytes)
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

    sig.v = recoverSignatureV(sig);

    validateSignatureDomain(sig);

    // Create recoverable signature
    std::vector< uint8_t > r = u256toBytes(sig.r);
    std::vector< uint8_t > s = u256toBytes(sig.s);
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
}


/// @brief  Convert a vector of bytes in big endian order to a u256 value.
/// @param bytes May be of any size. If > 32, then we only get the first 32 bytes.
/// If < 32, then we only read the first bytes
/// @return u256
u256 EthTransaction::bytesToU256(const std::vector<uint8_t>& bytes) {
    u256 val = 0;
    size_t bytes_size = bytes.size();
    
    for (size_t i = 0; i < bytes_size; ++i) {
        if (i >= 32) {
            // If more than 32 bytes, ignore the rest
            break;
        }
        val = (val << 8) | bytes[i];
    }
    return val;
}

/**
 * @brief Convert a u256 value to a vector of bytes in big-endian order.
 */
std::vector< uint8_t> EthTransaction::u256toBytes( u256 v_value ) {
    std::vector<uint8_t> bytes(32);
    for (size_t i = 0; i < 32; i++) {
        bytes[31 - i] = static_cast<uint8_t>(v_value & 0xFF);
        v_value >>= 8;
    }
    return bytes;
}

// -----------------------------------------------------------------------------------
//                                      Legacy
// -----------------------------------------------------------------------------------

std::vector< std::vector< uint8_t > > LegacyTx::encode() const {
    std::vector< std::vector< uint8_t > > fields;
    addEncodedFieldUint256(fields, nonce);
    addEncodedFieldUint256(fields, gasPrice);
    addEncodedFieldUint256(fields, gasLimit);
    addEncodedFieldBytes  (fields, to);
    addEncodedFieldUint256(fields, value);
    addEncodedFieldBytes  (fields, data);

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
        CHECK_STATE2(sig.v > 36, "Invalid signature. Expected <= 36, got: " + sig.v.str());

        u256 const chain = ( sig.v - 35 ) / 2;
        CHECK_STATE2( chain <= std::numeric_limits< uint64_t >::max(),
            "Invalid signature: Legacy Tx chainID overflow" );
        u256 recoveryId = sig.v - ( chain * 2 + 35 );        
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

std::vector< std::vector< uint8_t > > Type1Tx::encode() const {
    std::vector< std::vector< uint8_t > > fields;
    addEncodedFieldUint256(fields, chainId);   // new field for EIP-2930
    addEncodedFieldUint256(fields, nonce);
    addEncodedFieldUint256(fields, gasPrice);
    addEncodedFieldUint256(fields, gasLimit);
    addEncodedFieldBytes  (fields, to);
    addEncodedFieldUint256(fields, value);
    addEncodedFieldBytes  (fields, data);

    // encode access list - new field for EIP-2930
    std::vector< std::vector< uint8_t > > rlpAccessList;
    for ( const auto& accessTuple : accessList ) {
        std::vector< uint8_t > rlpAccessTuple = accessTuple.encode();
        rlpAccessList.push_back( rlpAccessTuple );
    }
    addEncodedFieldBytes(fields, std::vector<uint8_t>{});

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

std::vector< std::vector< uint8_t > > Type2Tx::encode() const {
    std::vector< std::vector< uint8_t > > fields;
    addEncodedFieldUint256(fields, chainId);
    addEncodedFieldUint256(fields, nonce);
    addEncodedFieldUint256(fields, maxPriorityFeePerGas);   // new field for EIP-1559
    addEncodedFieldUint256(fields, maxFeePerGas);           // new field for EIP-1559
    addEncodedFieldUint256(fields, gasLimit);
    addEncodedFieldBytes  (fields, to);
    addEncodedFieldUint256(fields, value);
    addEncodedFieldBytes  (fields, data);

    std::vector< std::vector< uint8_t > > rlpAccessList;
    for ( const auto& accessTuple : accessList ) {
        std::vector< uint8_t > rlpAccessTuple = accessTuple.encode();
        rlpAccessList.push_back( rlpAccessTuple );
    }
    addEncodedFieldList(fields, rlpAccessList);

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