#pragma once




class EthTransactionEncoder {

    using uint256 = std::vector< uint8_t >;

    struct LegacyTx {
        uint256 nonce;
        uint256 gasPrice;
        uint256 gasLimit;
        std::vector< uint8_t > to;  // 20 bytes
        uint256 value;
        std::vector< uint8_t > data;
        uint256 chainId;
    };


    static std::vector< uint8_t > generate_private_key();
    
    static std::vector< uint8_t > keccak256( const std::vector< uint8_t >& data );

    static std::vector< uint8_t > hash_transaction( const std::vector< uint8_t >& tx );


    static void rlp_encode_bytes( std::vector< uint8_t >& out, const std::vector< uint8_t >& data );


    static void rlp_encode_uint256( std::vector< uint8_t >& out, const std::vector< uint8_t >& value );

    static void rlp_encode_list(
        std::vector< uint8_t >& out, const std::vector< std::vector< uint8_t > >& elements );

    static std::vector< uint8_t > rlp_encode( const LegacyTx& tx, bool withSig,
        std::vector< uint8_t >* v_encoded, std::vector< uint8_t >* r_encoded,
        std::vector< uint8_t >* s_encoded );

    static ptr<vector<uint8_t>> signAndEncodeTx(const LegacyTx& tx);

public:

    static ptr<vector<uint8_t>> generateSampleTx(bool _isByte);


    static void uint64toVec( uint64_t v_value, vector< uint8_t >& v_vec );
    static void verifyEthSignature( const vector< uint8_t >& v_vec,
        const vector< uint8_t >& r_bytes, const vector< uint8_t >& s_bytes,
        const vector< uint8_t >& tx_hash );
};
