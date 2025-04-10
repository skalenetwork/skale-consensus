
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/rand.h>
#include <secp256k1.h>
#include <secp256k1_recovery.h>

#include <openssl/sha.h>

#include "SkaleCommon.h"
#include "Log.h"
#include "node/ConsensusInterface.h"
#include "bite/BiteDataFiled.h"
#include "libBLS/threshold_encryption/ThresholdEncryption.h"
#include "crypto/EncryptedAESKey.h"
#include "ParsedEthTransaction.h"
#include "EthTransactionEncoder.h"

#pragma GCC diagnostic push // make compiler happy
#pragma GCC diagnostic ignored "-Wignored-attributes"

auto EthTransactionEncoder::getHashContext() {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    CHECK_STATE( ctx );  // Assuming this is a macro or function to check nullptr
    return std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>( ctx, &EVP_MD_CTX_free);
}

auto EthTransactionEncoder::getSecp256k1SignContext() {
    secp256k1_context* raw = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    CHECK_STATE(raw);
    return std::unique_ptr<secp256k1_context, decltype(&secp256k1_context_destroy)>(
        raw, &secp256k1_context_destroy
    );
};

auto EthTransactionEncoder::getSecp256k1VerifyContext() {
    secp256k1_context* raw = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
    CHECK_STATE(raw);
    return std::unique_ptr<secp256k1_context, decltype(&secp256k1_context_destroy)>(
        raw, &secp256k1_context_destroy
    );
};

#pragma GCC diagnostic pop

std::vector< uint8_t > EthTransactionEncoder::keccak256( const std::vector< uint8_t >& data ) {
    std::vector< uint8_t > hash( 32 );
    thread_local auto ctx = getHashContext();
    EVP_DigestInit_ex( ctx.get(), EVP_sha3_256(), NULL );
    EVP_DigestUpdate( ctx.get(), data.data(), data.size() );
    EVP_DigestFinal_ex( ctx.get(), hash.data(), NULL );
    return hash;
}

std::vector< uint8_t > EthTransactionEncoder::hashTransaction( const std::vector< uint8_t >& tx ) {
    return keccak256( tx );
}


void EthTransactionEncoder::rlpEncodeBytes(
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
            throw std::invalid_argument( "rlpEncodeBytes: data too large (exceeds 2^64-1)" );
        }

        out.push_back( static_cast< uint8_t >( 0xb7 + len_bytes.size() ) );
        out.insert( out.end(), len_bytes.begin(), len_bytes.end() );
    }

    // 🔹 Append actual data
    out.insert( out.end(), data.begin(), data.end() );
}


void EthTransactionEncoder::rlpEncodeUint256(
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


void EthTransactionEncoder::rlpEncodeList(
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

std::vector< uint8_t > EthTransactionEncoder::rlpEncode( const LegacyTx& tx, bool withSig,
    std::vector< uint8_t >* v_encoded, std::vector< uint8_t >* r_encoded,
    std::vector< uint8_t >* s_encoded ) {
    std::vector< std::vector< uint8_t > > fields;
    std::vector< uint8_t > tmp;
    rlpEncodeUint256( tmp, tx.nonce );
    fields.push_back( tmp );
    tmp.clear();
    rlpEncodeUint256( tmp, tx.gasPrice );
    fields.push_back( tmp );
    tmp.clear();
    rlpEncodeUint256( tmp, tx.gasLimit );
    fields.push_back( tmp );
    tmp.clear();
    rlpEncodeBytes( tmp, tx.to );
    fields.push_back( tmp );
    tmp.clear();
    rlpEncodeUint256( tmp, tx.value );
    fields.push_back( tmp );
    tmp.clear();
    rlpEncodeBytes( tmp, tx.data );
    fields.push_back( tmp );
    tmp.clear();


    if ( withSig ) {
        if ( !v_encoded || !r_encoded || !s_encoded )
            throw std::invalid_argument( "Signature components are missing" );
        fields.push_back( *v_encoded );
        fields.push_back( *r_encoded );
        fields.push_back( *s_encoded );
    } else {
        std::vector< uint8_t > ZERO;
        rlpEncodeUint256( tmp, ZERO );
        fields.push_back( tmp );
        tmp.clear();
        rlpEncodeUint256( tmp, ZERO );
        fields.push_back( tmp );
        fields.push_back( tmp );
        tmp.clear();
    }

    std::vector< uint8_t > encoded;
    rlpEncodeList( encoded, fields );
    return encoded;
}





std::vector< uint8_t > EthTransactionEncoder::generateRandomPrivateKey() {
    std::vector< uint8_t > priv_key( 32 );

    if ( RAND_bytes( priv_key.data(), priv_key.size() ) != 1 ) {
        throw std::invalid_argument( "Failed to generate cryptographically secure private key" );
    }

    // Optional: verify key is valid for secp256k1
    thread_local auto ctx = getSecp256k1SignContext();
    if ( !secp256k1_ec_seckey_verify( ctx.get(), priv_key.data() ) ) {
        throw std::invalid_argument( "Generated private key is invalid for secp256k1" );
    }

    return priv_key;
}


void EthTransactionEncoder::verifyEthSignature( const vector< uint8_t >& v_vec,
    const vector< uint8_t >& r_bytes, const vector< uint8_t >& s_bytes,
    const vector< uint8_t >& tx_hash)
{
   if (r_bytes.size() != 32 || s_bytes.size() != 32 || tx_hash.size() != 32)
        throw std::invalid_argument("Invalid input sizes");

    // Convert v_vec to uint64_t
    uint64_t v = 0;
    for (uint8_t byte : v_vec) {
        v = (v << 8) | byte;
    }

    // Extract recovery ID from v (EIP-155)
    // v = chain_id * 2 + 35 + rec_id  → rec_id = v % 2
    int rec_id = static_cast<int>((v - 35) % 2);
    if (rec_id < 0 || rec_id > 3)
        throw std::invalid_argument("Invalid recovery ID");

    // Create recoverable signature
    uint8_t sig64[64];
    std::copy(r_bytes.begin(), r_bytes.end(), sig64);
    std::copy(s_bytes.begin(), s_bytes.end(), sig64 + 32);

    secp256k1_ecdsa_recoverable_signature signature;

    // Static secp256k1 context for verification. A single contect for each thread
    // this is because context is not thread safe
    thread_local auto ctx = getSecp256k1VerifyContext();


    if (!secp256k1_ecdsa_recoverable_signature_parse_compact(ctx.get(), &signature, sig64, rec_id)) {
        throw std::invalid_argument("Failed to parse recoverable signature");
    }

    // Recover public key from signature
    secp256k1_pubkey pubkey;
    if (!secp256k1_ecdsa_recover(ctx.get(), &pubkey, &signature, tx_hash.data())) {
        throw std::invalid_argument("Failed to recover public key from signature");
    }

    // Convert to normal signature and verify
    secp256k1_ecdsa_signature normal_sig;
    secp256k1_ecdsa_recoverable_signature_convert(ctx.get(), &normal_sig, &signature);

    int verified = secp256k1_ecdsa_verify(ctx.get(), &normal_sig, tx_hash.data(), &pubkey);
    if (verified != 1) {
        throw std::invalid_argument("Signature did not verify");
    }
}

ptr< vector< uint8_t > > EthTransactionEncoder::signAndEncodeTx( const LegacyTx& tx ) {
    std::vector< uint8_t > privkey = generateRandomPrivateKey();

    // 1. RLP encode transaction with EIP-155 format (with chainId, 0, 0)
    std::vector< uint8_t > encoded_tx = rlpEncode( tx, false, nullptr, nullptr, nullptr );
    std::vector< uint8_t > tx_hash = hashTransaction( encoded_tx );
    if ( tx_hash.size() != 32 ) {
        throw std::invalid_argument( "Invalid transaction hash size" );
    }


    // thread local because it is not thread safe, but can be reusef
    auto ctx = EthTransactionEncoder::getSecp256k1SignContext();


    // 3. Sign hash using recoverable signature
    secp256k1_ecdsa_recoverable_signature signature;
    if ( !secp256k1_ecdsa_sign_recoverable(
             ctx.get(), &signature, tx_hash.data(), privkey.data(), nullptr, nullptr ) ) {
        throw std::invalid_argument( "Failed to sign transaction" );
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
    rlpEncodeUint256( v_encoded, v_vec );
    rlpEncodeUint256( r_encoded, r_bytes );
    rlpEncodeUint256( s_encoded, s_bytes );

    auto result = rlpEncode( tx, true, &v_encoded, &r_encoded, &s_encoded );

    verifyEthSignature(v_vec, r_bytes, s_bytes, tx_hash);

    return make_shared< vector< uint8_t > >( std::move( result ) );
}
void EthTransactionEncoder::uint64toVec( uint64_t v_value, vector< uint8_t >& v_vec ) {
    while ( v_value > 0 ) {
        v_vec.insert( v_vec.begin(), static_cast< uint8_t >( v_value & 0xFF ) );
        v_value >>= 8;
    }
}

ptr< vector< uint8_t > > EthTransactionEncoder::generateSampleTx( bool _isByte ) {
    static atomic< uint64_t > nonce = 0;


    static std::unique_ptr< LegacyTx > templateTx = std::make_unique< LegacyTx >( LegacyTx{
        {},                                            // nonce
        { 0x3b, 0x9a, 0xca, 0x00 },                    // gasPrice
        { 0x52, 0x08 },                                // gasLimit
        std::vector< uint8_t >( 20, 0x12 ),            // to
        { 0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64, 0x00 },  // value
        { 0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64, 0x00, 0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64,
            0x00 },  // data
        {},          // chainId
    } );


    LegacyTx currentTx = *templateTx;

    auto currentNonce = nonce.fetch_add( 1 );

    uint64toVec( BITE_CHAIN_ID, currentTx.chainId );

    uint64toVec( currentNonce, currentTx.nonce );

    auto encryptedData = libBLS::ThresholdEncryption::mockupEncrypt(currentTx.data);

    auto encryptedKeyBytes = make_shared<array<uint8_t , BITE_ENCRYPTED_AES_KEY_LEN>>();

    auto encryptedAesKey = make_shared<EncryptedAESKey>(encryptedKeyBytes);


    if ( _isByte ) {
        BiteDataField biteDataField(
            encryptedAesKey, make_shared< EncryptedData >( encryptedData ), 0, true);
        currentTx.data = *biteDataField.getSerializedData();
    }

    auto encodedTx = signAndEncodeTx( currentTx );
    CHECK_STATE( encodedTx );

    return encodedTx;
}
ptr< vector< uint8_t > >  EthTransactionEncoder::rlpEncodeWithoutSig(
    ParsedEthTransaction& _ethTransaction ) {
    auto fields = _ethTransaction.getFields();
    LegacyTx tx;
    tx.nonce = fields.at(0);
    tx.gasPrice = fields.at(1);
    tx.gasLimit = fields.at(2);
    tx.to = fields.at(3);
    tx.value = fields.at(4);
    tx.data = fields.at(5);

    auto result =  rlpEncode(tx, false, nullptr, nullptr, nullptr );

    return make_shared<vector< uint8_t >>(result);
}
