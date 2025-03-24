#include "SkaleCommon.h"

#include "ParsedEthTransaction.h"


#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/bn.h>
#include <iostream>
#include <vector>
#include <random>
#include <cstring>
#include <variant>
#include <optional>
#include <sstream>
#include <array>

using uint256 = std::vector<uint8_t>;

// --- Transaction Types ---

struct LegacyTx {
    uint256 nonce;
    uint256 gasPrice;
    uint256 gasLimit;
    std::vector<uint8_t> to; // 20 bytes
    uint256 value;
    std::vector<uint8_t> data;
};

struct AccessListEntry {
    std::vector<uint8_t> address; // 20 bytes
    std::vector<std::vector<uint8_t>> storageKeys; // 32-byte keys
};

using AccessList = std::vector<AccessListEntry>;

struct EIP2930Tx {
    uint256 chainId;
    uint256 nonce;
    uint256 gasPrice;
    uint256 gasLimit;
    std::vector<uint8_t> to;
    uint256 value;
    std::vector<uint8_t> data;
    AccessList accessList;
};

struct EIP1559Tx {
    uint256 chainId;
    uint256 nonce;
    uint256 maxPriorityFeePerGas;
    uint256 maxFeePerGas;
    uint256 gasLimit;
    std::vector<uint8_t> to;
    uint256 value;
    std::vector<uint8_t> data;
    AccessList accessList;
};

struct EIP4844Tx {
    uint256 chainId;
    uint256 nonce;
    uint256 maxPriorityFeePerGas;
    uint256 maxFeePerGas;
    uint256 gasLimit;
    std::vector<uint8_t> to;
    uint256 value;
    std::vector<uint8_t> data;
    AccessList accessList;
    uint256 maxFeePerBlobGas;
    std::vector<std::vector<uint8_t>> blobVersionedHashes;
};

using EthereumTx = std::variant<LegacyTx, EIP2930Tx, EIP1559Tx, EIP4844Tx>;

// --- Helpers ---

std::vector<uint8_t> generate_private_key() {
    std::random_device rd;
    std::vector<uint8_t> priv_key(32);
    for (int i = 0; i < 32; ++i) {
        priv_key[i] = rd() % 256;
    }
    return priv_key;
}

std::vector<uint8_t> keccak256(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> hash(32);
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha3_256(), NULL);
    EVP_DigestUpdate(ctx, data.data(), data.size());
    EVP_DigestFinal_ex(ctx, hash.data(), NULL);
    EVP_MD_CTX_free(ctx);
    return hash;
}

std::vector<uint8_t> hash_transaction(const std::vector<uint8_t>& tx) {
    return keccak256(tx);
}

void rlp_encode_uint256(std::vector<uint8_t>& out, const uint256& value) {
    size_t start = 0;
    while (start < value.size() && value[start] == 0) ++start;
    size_t len = value.size() - start;
    if (len == 0) {
        out.push_back(0x80);
        return;
    }
    if (len == 1 && value[start] < 0x80) {
        out.push_back(value[start]);
    } else {
        out.push_back(0x80 + len);
        out.insert(out.end(), value.begin() + start, value.end());
    }
}


void rlp_encode_bytes(std::vector<uint8_t>& out, const std::vector<uint8_t>& data) {
    if (data.size() == 1 && data[0] < 0x80) {
        out.push_back(data[0]);
        return;
    }
    if (data.size() < 56) {
        out.push_back(0x80 + data.size());
    } else {
        std::vector<uint8_t> len;
        size_t sz = data.size();
        while (sz) {
            len.insert(len.begin(), static_cast<uint8_t>(sz & 0xFF));
            sz >>= 8;
        }
        out.push_back(0xb7 + len.size());
        out.insert(out.end(), len.begin(), len.end());
    }
    out.insert(out.end(), data.begin(), data.end());
}

void rlp_encode_list(std::vector<uint8_t>& out, const std::vector<std::vector<uint8_t>>& elements) {
    std::vector<uint8_t> payload;
    for (const auto& e : elements) {
        payload.insert(payload.end(), e.begin(), e.end());
    }
    if (payload.size() < 56) {
        out.push_back(0xc0 + payload.size());
    } else {
        std::vector<uint8_t> len;
        size_t sz = payload.size();
        while (sz) {
            len.insert(len.begin(), static_cast<uint8_t>(sz & 0xFF));
            sz >>= 8;
        }
        out.push_back(0xf7 + len.size());
        out.insert(out.end(), len.begin(), len.end());
    }
    out.insert(out.end(), payload.begin(), payload.end());
}

std::vector<uint8_t> rlp_encode(const EthereumTx& tx) {
    std::vector<std::vector<uint8_t>> fields;
    std::visit([&](auto&& t) {
        using T = std::decay_t<decltype(t)>;
        std::vector<uint8_t> tmp;
        if constexpr (std::is_same_v<T, LegacyTx>) {
            rlp_encode_uint256(tmp, t.nonce); fields.push_back(tmp); tmp.clear();
            rlp_encode_uint256(tmp, t.gasPrice); fields.push_back(tmp); tmp.clear();
            rlp_encode_uint256(tmp, t.gasLimit); fields.push_back(tmp); tmp.clear();
            rlp_encode_bytes(tmp, t.to); fields.push_back(tmp); tmp.clear();
            rlp_encode_uint256(tmp, t.value); fields.push_back(tmp); tmp.clear();
            rlp_encode_bytes(tmp, t.data); fields.push_back(tmp); tmp.clear();
        }
        else if constexpr (std::is_same_v<T, EIP1559Tx>) {
            rlp_encode_uint256(tmp, t.chainId); fields.push_back(tmp); tmp.clear();
            rlp_encode_uint256(tmp, t.nonce); fields.push_back(tmp); tmp.clear();
            rlp_encode_uint256(tmp, t.maxPriorityFeePerGas); fields.push_back(tmp); tmp.clear();
            rlp_encode_uint256(tmp, t.maxFeePerGas); fields.push_back(tmp); tmp.clear();
            rlp_encode_uint256(tmp, t.gasLimit); fields.push_back(tmp); tmp.clear();
            rlp_encode_bytes(tmp, t.to); fields.push_back(tmp); tmp.clear();
            rlp_encode_uint256(tmp, t.value); fields.push_back(tmp); tmp.clear();
            rlp_encode_bytes(tmp, t.data); fields.push_back(tmp); tmp.clear();
        }
    }, tx);
    std::vector<uint8_t> encoded;
    rlp_encode_list(encoded, fields);
    return encoded;
}


std::vector<std::vector<uint8_t>> rlp_extract_fields(const EthereumTx& tx) {
    std::vector<std::vector<uint8_t>> fields;

    std::visit([&](auto&& t) {
        using T = std::decay_t<decltype(t)>;

        auto push_uint = [&](const uint256& val) {
            std::vector<uint8_t> tmp;
            rlp_encode_uint256(tmp, val);
            fields.push_back(std::move(tmp));
        };

        auto push_bytes = [&](const std::vector<uint8_t>& val) {
            std::vector<uint8_t> tmp;
            rlp_encode_bytes(tmp, val);
            fields.push_back(std::move(tmp));
        };

        if constexpr (std::is_same_v<T, LegacyTx>) {
            push_uint(t.nonce);
            push_uint(t.gasPrice);
            push_uint(t.gasLimit);
            push_bytes(t.to);
            push_uint(t.value);
            push_bytes(t.data);
        }

        else if constexpr (std::is_same_v<T, EIP1559Tx>) {
            push_uint(t.chainId);
            push_uint(t.nonce);
            push_uint(t.maxPriorityFeePerGas);
            push_uint(t.maxFeePerGas);
            push_uint(t.gasLimit);
            push_bytes(t.to);
            push_uint(t.value);
            push_bytes(t.data);

            // Optional: encode access list if implemented
            // push_access_list(t.accessList);
        }

        else if constexpr (std::is_same_v<T, EIP2930Tx>) {
            push_uint(t.chainId);
            push_uint(t.nonce);
            push_uint(t.gasPrice);
            push_uint(t.gasLimit);
            push_bytes(t.to);
            push_uint(t.value);
            push_bytes(t.data);
            // push_access_list(t.accessList);
        }

        else if constexpr (std::is_same_v<T, EIP4844Tx>) {
            push_uint(t.chainId);
            push_uint(t.nonce);
            push_uint(t.maxPriorityFeePerGas);
            push_uint(t.maxFeePerGas);
            push_uint(t.gasLimit);
            push_bytes(t.to);
            push_uint(t.value);
            push_bytes(t.data);
            // push_access_list(t.accessList);
            push_uint(t.maxFeePerBlobGas);
            // push_blob_versioned_hashes(t.blobVersionedHashes);
        }

    }, tx);

    return fields;
}



bool sign_transaction_generic(
    const EthereumTx& tx,
    std::vector<uint8_t>& out_signed_tx,
    std::vector<uint8_t>& out_privkey
) {
    out_privkey = generate_private_key();

    // 1. RLP encode transaction without signature
    std::vector<uint8_t> encoded_tx = rlp_encode(tx);
    std::vector<uint8_t> tx_hash = hash_transaction(encoded_tx);

    // 2. Create EC key and private BIGNUM
    EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!ec_key) throw std::runtime_error("Failed to create EC key");

    BIGNUM* priv_bn = BN_bin2bn(out_privkey.data(), out_privkey.size(), NULL);
    if (!priv_bn) {
        EC_KEY_free(ec_key);
        throw std::runtime_error("Failed to create private key BIGNUM");
    }

    if (!EC_KEY_set_private_key(ec_key, priv_bn)) {
        BN_free(priv_bn);
        EC_KEY_free(ec_key);
        throw std::runtime_error("Failed to set private key on EC_KEY");
    }

    // 3. Derive and set public key
    EC_POINT* pub_key = EC_POINT_new(EC_KEY_get0_group(ec_key));
    if (!pub_key || !EC_POINT_mul(EC_KEY_get0_group(ec_key), pub_key, priv_bn, NULL, NULL, NULL) ||
         !EC_KEY_set_public_key(ec_key, pub_key)) {
        EC_POINT_free(pub_key);
        BN_free(priv_bn);
        EC_KEY_free(ec_key);
        throw std::runtime_error("Failed to compute/set public key");
    }
    EC_POINT_free(pub_key);

    // 4. Sign the hash
    ECDSA_SIG* sig = ECDSA_do_sign(tx_hash.data(), tx_hash.size(), ec_key);
    if (!sig) {
        BN_free(priv_bn);
        EC_KEY_free(ec_key);
        return false;
    }

    // 5. Extract r and s
    const BIGNUM *r = nullptr, *s = nullptr;
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    r = sig->r;
    s = sig->s;
#else
    ECDSA_SIG_get0(sig, &r, &s);
#endif

    std::vector<uint8_t> r_bytes(32);
    std::vector<uint8_t> s_bytes(32);
    BN_bn2binpad(r, r_bytes.data(), 32);
    BN_bn2binpad(s, s_bytes.data(), 32);

    // 6. Compute simplified v (real recovery ID logic is optional)
    uint8_t v = 27;  // or: chain_id * 2 + 35 if using EIP-155

    // 7. RLP encode signed transaction
    std::vector<uint8_t> v_encoded, r_encoded, s_encoded;
    rlp_encode_uint256(v_encoded, {v});
    rlp_encode_uint256(r_encoded, r_bytes);
    rlp_encode_uint256(s_encoded, s_bytes);

    std::vector<std::vector<uint8_t>> fields = rlp_extract_fields(tx);
    fields.push_back(v_encoded);
    fields.push_back(r_encoded);
    fields.push_back(s_encoded);

    out_signed_tx.clear();
    rlp_encode_list(out_signed_tx, fields);

    // 8. Cleanup
    ECDSA_SIG_free(sig);
    BN_free(priv_bn);
    EC_KEY_free(ec_key);
    return true;
}

bool ParsedEthTransaction::isTypedTransaction( uint8_t prefix ) {
    return prefix == 0x01 || prefix == 0x02;
}

size_t ParsedEthTransaction::readLen(
    const std::vector< uint8_t >& _tx, size_t _offset, size_t _len ) {
    if ( _offset + _len > _tx.size() ) {
        throw std::runtime_error( "readLen: out of bounds" );
    }
    size_t val = 0;
    for ( size_t i = 0; i < _len; ++i ) {
        val = ( val << 8 ) | _tx.at( _offset + i );
    }
    return val;
}

void ParsedEthTransaction::skipRlpListHeader( const std::vector< uint8_t >& _tx, uint64_t& _offset ) {
    if ( _offset >= _tx.size() )
        throw std::runtime_error( "skipRlpListHeader: no bytes left" );
    uint8_t prefix = _tx.at( _offset );
    if ( prefix <= 0xf7 ) {
        _offset += 1;
    } else if ( prefix <= 0xff ) {
        size_t lenOfLen = prefix - 0xf7;
        if ( _offset + 1 + lenOfLen > _tx.size() ) {
            throw std::runtime_error( "skipRlpListHeader: out of bounds" );
        }
        _offset += 1 + lenOfLen;
    } else {
        throw invalid_argument( "Invalid RLP list prefix" );
    }
}

std::vector< uint8_t > ParsedEthTransaction::parseSingleByteVector(
    const std::vector< uint8_t >& _tx, uint64_t& _offset ) {
    if ( _offset >= _tx.size() )
        throw std::runtime_error( "Short element: out of bounds" );
    return { _tx.at( _offset++ ) };
}

std::vector< uint8_t > ParsedEthTransaction::parseShortByteVector(
    const std::vector< uint8_t >& _tx, uint64_t& _offset, uint8_t _prefix ) {
    size_t len = _prefix - 0x80;
    _offset += 1;
    if ( _offset + len > _tx.size() ) {
        throw std::runtime_error( "parseShortByteVector: slice out of bounds" );
    }
    std::vector< uint8_t > out( _tx.begin() + _offset, _tx.begin() + _offset + len );
    _offset += len;
    return out;
}


std::vector< uint8_t > ParsedEthTransaction::parseLongByteVector(
    const std::vector< uint8_t >& _tx, uint64_t& _offset, uint8_t _prefix ) {
    size_t lenOfLen = _prefix - 0xb7;
    if ( _offset + 1 + lenOfLen > _tx.size() ) {
        throw std::runtime_error( "parseLongByteVector: lenOfLen out of bounds" );
    }
    size_t len = readLen( _tx, _offset + 1, lenOfLen );
    _offset += 1 + lenOfLen;
    if ( _offset + len > _tx.size() ) {
        throw std::runtime_error( "parseLongByteVector: slice out of bounds" );
    }
    std::vector< uint8_t > out( _tx.begin() + _offset, _tx.begin() + _offset + len );
    _offset += len;
    return out;
}

std::vector< uint8_t > ParsedEthTransaction::parseByteVector(
    const std::vector< uint8_t >& _tx, uint64_t& _offset ) {
    if ( _offset >= _tx.size() )
        throw std::runtime_error( "parseByteVector: no data left" );
    uint8_t prefix = _tx.at( _offset );
    if ( prefix <= 0x7f )
        return parseSingleByteVector( _tx, _offset );
    else if ( prefix <= 0xb7 )
        return parseShortByteVector( _tx, _offset, prefix );
    else if ( prefix <= 0xbf )
        return parseLongByteVector( _tx, _offset, prefix );
    else
        throw invalid_argument( "Invalid RLP element prefix" );
}

void ParsedEthTransaction::parseTransactionFields(
    const std::vector< uint8_t >& _tx, size_t& _offset, int fieldCount ) {
    for ( int i = 0; i < fieldCount; ++i ) {
        fields.push_back( parseByteVector( _tx, _offset ) );
    }

    if ( _offset != _tx.size() ) {
        throw invalid_argument( "Too many fields in transaction" );
    }
}

ptr< ParsedEthTransaction > ParsedEthTransaction::parse( const std::vector< uint8_t >& _rawTx ) {
    if ( _rawTx.empty() ) {
        throw invalid_argument( "Empty transaction" );
    }

    auto result = make_shared< ParsedEthTransaction >();
    size_t offset = 0;
    uint8_t prefix = _rawTx.at( offset );

    if ( isTypedTransaction( prefix ) ) {
        result->type = prefix;
        offset += 1;
        skipRlpListHeader( _rawTx, offset );
        int fieldCount = ( prefix == 0x01 ) ? 11 : 12;
        result->parseTransactionFields( _rawTx, offset, fieldCount );
    } else if ( prefix >= 0xc0 ) {
        result->type = 0;
        skipRlpListHeader( _rawTx, offset );
        result->parseTransactionFields( _rawTx, offset, 9 );
    } else {
        throw invalid_argument( "Invalid transaction prefix" );
    }

    result->validateAll();

    return result;
}

void ParsedEthTransaction::validateAll() {
    validateFieldsCount();
    validateToField();
    validateSignature();
}
void ParsedEthTransaction::validateFieldsCount() const {
    size_t expectedFields = 0;
    switch ( type ) {
    case 0:
        expectedFields = 9;
        break;
    case 1:
        expectedFields = 11;
        break;
    case 2:
        expectedFields = 12;
        break;
    default:
        throw invalid_argument( "Unknown transaction type" );
    }
    if ( fields.size() != expectedFields ) {
        throw invalid_argument( "Incorrect number of fields" );
    }
}
void ParsedEthTransaction::validateToField() {
    const auto& toField = fields.at( type == 0 ? 3 : 5 );
    if ( !toField.empty() && toField.size() != 20 ) {
        throw invalid_argument( "Invalid 'to' address length" );
    }
}

inline bool ParsedEthTransaction::isZero( const std::vector< uint8_t >& _data ) {
    for ( uint8_t byte : _data ) {
        if ( byte != 0 )
            return false;
    }
    return true;
}

void ParsedEthTransaction::validateSignature() {
    const auto& r = fields.at( fields.size() - 2 );
    const auto& s = fields.at( fields.size() - 1 );
    if ( r.size() > 32 || s.size() > 32 ) {
        throw invalid_argument( "Invalid r/s size (should be <= 32 bytes)" );
    }

    if ( isZero( r ) || isZero( s ) ) {
        throw invalid_argument( "Zero r/s" );
    }
}

ptr< std::vector< uint8_t > > ParsedEthTransaction::getTransactionDataField() {
    size_t index;
    switch ( type ) {
    case 0:
        index = 5;
        break;
    case 1:
    case 2:
        index = 7;
        break;
    default:
        throw invalid_argument( "Unknown transaction type" );
    }
    if ( fields.size() <= index ) {
        throw invalid_argument( "Transaction missing data field" );
    }
    return make_shared< vector< uint8_t > >( fields.at( index ) );
}


void ParsedEthTransaction::testEthereumTxParser() {
    std::vector< std::vector< uint8_t > > testTxs = { // Legacy transaction (valid)
        { 0xf8, 0x66, 0x82, 0x01, 0xf4, 0x84, 0x3b, 0x9a, 0xca, 0x00, 0x83, 0x01, 0x86, 0xa0, 0x94,
            0xd8, 0x94, 0xd9, 0x96, 0x83, 0xb2, 0x74, 0xc3, 0x86, 0xee, 0xe2, 0x7b, 0xb1, 0x17,
            0xf1, 0x99, 0x01, 0x80, 0x1b, 0xa0, 0x6e, 0x87, 0x69, 0xb0, 0x90, 0x8e, 0x0f, 0xd6,
            0x46, 0x8d, 0xa0, 0x69, 0x85, 0x39, 0x6f, 0xf6, 0x77, 0x6b, 0x2a, 0xb7, 0xb3, 0x5f,
            0x82, 0xe9, 0xb6, 0xb3, 0x47, 0xea, 0x1b, 0xa0, 0x52, 0x99, 0x55, 0xaa, 0x99, 0x9c,
            0x35, 0x13, 0x39, 0x44, 0xfa, 0x73, 0xe2, 0xdd, 0x7d, 0x4b, 0xd3, 0x0d, 0x6b, 0x30,
            0xa3, 0xc4, 0x5f, 0x3e, 0xf6, 0x44, 0x1d, 0x6d, 0x89, 0xa6 },

        // EIP-2930 transaction (valid)
        { 0x01, 0xf8, 0x44, 0x82, 0x01, 0xf4, 0x84, 0x3b, 0x9a, 0xca, 0x00, 0x83, 0x01, 0x86, 0xa0,
            0x94, 0xd8, 0x94, 0xd9, 0x96, 0x83, 0xb2, 0x74, 0xc3, 0x86, 0xee, 0xe2, 0x7b, 0xb1,
            0x17, 0xf1, 0x99, 0x01, 0x80, 0xc0, 0x1b, 0xa0, 0x6e, 0x87, 0x69, 0xb0, 0x90, 0x8e,
            0x0f, 0xd6, 0x46, 0x8d, 0xa0, 0x69, 0x85, 0x39, 0x6f, 0xf6, 0x77, 0x6b, 0x2a, 0xb7,
            0xb3, 0x5f, 0x82, 0xe9, 0xb6, 0xb3, 0x47, 0xea, 0x1b },

        // EIP-1559 transaction (valid)
        { 0x02, 0xf8, 0x4a, 0x01, 0x85, 0x04, 0xa8, 0x1e, 0x00, 0x85, 0x04, 0xa8, 0x1e, 0x00, 0x82,
            0x01, 0xf4, 0x94, 0xd8, 0x94, 0xd9, 0x96, 0x83, 0xb2, 0x74, 0xc3, 0x80, 0xc0, 0x1b,
            0xa0, 0x6e, 0x87, 0x69, 0xb0, 0x90, 0x8e, 0x0f, 0xd6, 0x46, 0x8d, 0xa0, 0x69, 0x85,
            0x39, 0x6f, 0xf6, 0x77, 0x6b, 0x2a, 0xb7, 0xb3, 0x5f, 0x82, 0xe9, 0xb6, 0xb3, 0x47,
            0xea, 0x1b },

        // Malformed (bad) transaction - truncated
        { 0xf8, 0x02, 0x82 } };

    for ( size_t i = 0; i < testTxs.size(); ++i ) {
        try {
            auto tx = parse( testTxs[i] );
        } catch ( const std::exception& e ) {
            std::cout << "Transaction " << i << ": parse failed - " << e.what() << "\n";
        }
    }
}

