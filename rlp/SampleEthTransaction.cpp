
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/rand.h>
#include <secp256k1.h>
#include <secp256k1_recovery.h>
#include <openssl/sha.h>

#include "SkaleCommon.h"
#include "Log.h"
#include "node/ConsensusInterface.h"
#include "SampleEthTransaction.h"


std::vector< uint8_t > SampleEthTransaction::keccak256( const std::vector< uint8_t >& data ) {
    std::vector< uint8_t > hash( 32 );
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex( ctx, EVP_sha3_256(), NULL );
    EVP_DigestUpdate( ctx, data.data(), data.size() );
    EVP_DigestFinal_ex( ctx, hash.data(), NULL );
    EVP_MD_CTX_free( ctx );
    return hash;
}

std::vector< uint8_t > SampleEthTransaction::hash_transaction( const std::vector< uint8_t >& tx ) {
    return keccak256( tx );
}


void SampleEthTransaction::rlp_encode_bytes(
    std::vector< uint8_t >& out, const std::vector< uint8_t >& data ) {
    const size_t len = data.size();

    // 🔹 Explicitly handle empty string
    if ( len == 0 ) {
        out.push_back( 0x80 );
        return;
    }

    // 🔹 Single byte < 0x80 is encoded directly (no prefix)
    if ( len == 1 && data[0] < 0x80 ) {
        out.push_back( data[0] );
        return;
    }

    if ( len < 56 ) {
        // 🔹 Short string: prefix = 0x80 + len
        out.push_back( static_cast< uint8_t >( 0x80 + len ) );
    } else {
        // 🔹 Long string: prefix = 0xb7 + len-of-len, then len, then data
        std::vector< uint8_t > len_bytes;
        size_t sz = len;
        while ( sz > 0 ) {
            len_bytes.insert( len_bytes.begin(), static_cast< uint8_t >( sz & 0xFF ) );
            sz >>= 8;
        }

        if ( len_bytes.size() > 8 ) {
            throw std::runtime_error( "rlp_encode_bytes: data too large (exceeds 2^64-1)" );
        }

        out.push_back( static_cast< uint8_t >( 0xb7 + len_bytes.size() ) );
        out.insert( out.end(), len_bytes.begin(), len_bytes.end() );
    }

    // 🔹 Append actual data
    out.insert( out.end(), data.begin(), data.end() );
}


void SampleEthTransaction::rlp_encode_uint256(
    std::vector< uint8_t >& out, const std::vector< uint8_t >& value ) {
    // Skip leading zeros
    size_t start = 0;
    while ( start < value.size() && value[start] == 0 ) {
        ++start;
    }

    // Extract minimal non-zero representation (or empty)
    std::vector< uint8_t > stripped( value.begin() + start, value.end() );

    // Delegate to rlp_encode_bytes
    rlp_encode_bytes( out, stripped );
}


void SampleEthTransaction::rlp_encode_list(
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

std::vector< uint8_t > SampleEthTransaction::rlp_encode( const LegacyTx& tx, bool withSig,
    std::vector< uint8_t >* v_encoded, std::vector< uint8_t >* r_encoded,
    std::vector< uint8_t >* s_encoded ) {
    std::vector< std::vector< uint8_t > > fields;
    std::vector< uint8_t > tmp;
    rlp_encode_uint256( tmp, tx.nonce );
    fields.push_back( tmp );
    tmp.clear();
    rlp_encode_uint256( tmp, tx.gasPrice );
    fields.push_back( tmp );
    tmp.clear();
    rlp_encode_uint256( tmp, tx.gasLimit );
    fields.push_back( tmp );
    tmp.clear();
    rlp_encode_bytes( tmp, tx.to );
    fields.push_back( tmp );
    tmp.clear();
    rlp_encode_uint256( tmp, tx.value );
    fields.push_back( tmp );
    tmp.clear();
    rlp_encode_bytes( tmp, tx.data );
    fields.push_back( tmp );


    if ( withSig ) {
        if ( !v_encoded || !r_encoded || !s_encoded )
            throw std::runtime_error( "Signature components are missing" );
        fields.push_back( *v_encoded );
        fields.push_back( *r_encoded );
        fields.push_back( *s_encoded );
    } else {
        rlp_encode_uint256( tmp, tx.chainId );
        fields.push_back( tmp );
        tmp.clear();
        std::vector< uint8_t > ZERO;
        rlp_encode_uint256( tmp, ZERO );
        fields.push_back( tmp );
        fields.push_back( tmp );
        tmp.clear();
    }

    std::vector< uint8_t > encoded;
    rlp_encode_list( encoded, fields );
    return encoded;
}


std::vector< uint8_t > SampleEthTransaction::generate_private_key() {
    std::vector< uint8_t > priv_key( 32 );

    if ( RAND_bytes( priv_key.data(), priv_key.size() ) != 1 ) {
        throw std::runtime_error( "Failed to generate cryptographically secure private key" );
    }

    // Optional: verify key is valid for secp256k1
    secp256k1_context* ctx = secp256k1_context_create( SECP256K1_CONTEXT_SIGN );
    if ( !secp256k1_ec_seckey_verify( ctx, priv_key.data() ) ) {
        secp256k1_context_destroy( ctx );
        throw std::runtime_error( "Generated private key is invalid for secp256k1" );
    }
    secp256k1_context_destroy( ctx );

    return priv_key;
}

ptr< vector< uint8_t > > SampleEthTransaction::signAndEncodeTx( const LegacyTx& tx ) {
    std::vector< uint8_t > privkey = generate_private_key();

    // 1. RLP encode transaction with EIP-155 format (with chainId, 0, 0)
    std::vector< uint8_t > encoded_tx = rlp_encode( tx, false, nullptr, nullptr, nullptr );
    std::vector< uint8_t > tx_hash = hash_transaction( encoded_tx );
    if ( tx_hash.size() != 32 ) {
        throw std::runtime_error( "Invalid transaction hash size" );
    }

    // unique pointer with autocleanup
    auto ctx = std::unique_ptr< secp256k1_context, decltype( &secp256k1_context_destroy ) >(
        secp256k1_context_create( SECP256K1_CONTEXT_SIGN ), &secp256k1_context_destroy );

    if ( !ctx )
        throw std::runtime_error( "Failed to create secp256k1 context" );

    // 3. Sign hash using recoverable signature
    secp256k1_ecdsa_recoverable_signature signature;
    if ( !secp256k1_ecdsa_sign_recoverable(
             ctx.get(), &signature, tx_hash.data(), privkey.data(), nullptr, nullptr ) ) {
        throw std::runtime_error( "Failed to sign transaction" );
    }

    // 4. Extract r, s, recovery ID
    uint8_t sig64[64];
    int rec_id = 0;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(
        ctx.get(), sig64, &rec_id, &signature );

    std::vector< uint8_t > r_bytes( sig64, sig64 + 32 );
    std::vector< uint8_t > s_bytes( sig64 + 32, sig64 + 64 );

    // 5. Compute correct `v` as per EIP-155
    uint64_t chain_id = 0;
    for ( uint8_t byte : tx.chainId ) {
        chain_id = ( chain_id << 8 ) | byte;
    }
    uint64_t v_value = chain_id * 2 + 35 + rec_id;
    // convert to vector
    std::vector< uint8_t > v_vec;
    uint64toVec( v_value, v_vec );

    // 6. RLP encode final signed transaction
    std::vector< uint8_t > v_encoded, r_encoded, s_encoded;
    rlp_encode_uint256( v_encoded, v_vec );
    rlp_encode_uint256( r_encoded, r_bytes );
    rlp_encode_uint256( s_encoded, s_bytes );

    auto result = rlp_encode( tx, true, &v_encoded, &r_encoded, &s_encoded );

    return make_shared< vector< uint8_t > >( std::move( result ) );
}
void SampleEthTransaction::uint64toVec( uint64_t v_value, vector< uint8_t >& v_vec ) {
    while ( v_value > 0 ) {
        v_vec.insert( v_vec.begin(), static_cast< uint8_t >( v_value & 0xFF ) );
        v_value >>= 8;
    }
}

ptr< vector< uint8_t > > SampleEthTransaction::generateSampleTx() {
    static atomic<uint64_t>  nonce = 0;

    static std::unique_ptr<LegacyTx> templateTx = std::make_unique<LegacyTx>(LegacyTx{
        {},                          // chainId
        {},                          // nonce
        { 0x3b, 0x9a, 0xca, 0x00 },  // gasPrice
        { 0x52, 0x08 },              // gasLimit
        std::vector<uint8_t>(20, 0x12),  // to
        { 0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64, 0x00 },  // value
        { 0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64, 0x00,
            0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64, 0x00 }   // data
    });



    auto currentTx = *templateTx;

    auto currentNonce = nonce.fetch_add(1);

    uint64toVec(BITE_CHAIN_ID, currentTx.chainId);

    uint64toVec(currentNonce, currentTx.nonce);

    std::cout << "Signed Transaction: ";
    auto encodedTx = signAndEncodeTx( currentTx);
    CHECK_STATE( encodedTx );
    for ( auto b : *encodedTx )
        std::printf( "%02x", b );

    return encodedTx;
}