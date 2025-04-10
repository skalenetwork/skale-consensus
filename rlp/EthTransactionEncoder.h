#pragma once

class ParsedEthTransaction;


class EthTransactionEncoder {

    using uint256 = std::vector< uint8_t >;


public:

    struct LegacyTx {
        uint256 nonce;
        uint256 gasPrice;
        uint256 gasLimit;
        std::vector< uint8_t > to;  // 20 bytes
        uint256 value;
        std::vector< uint8_t > data;
        uint256 chainId;
    };

    static ptr<vector<uint8_t>> generateSampleTx(bool _isByte);

    static void uint64toVec( uint64_t v_value, vector< uint8_t >& v_vec );

    static void verifyEthSignature( const vector< uint8_t >& v_vec,
        const vector< uint8_t >& r_bytes, const vector< uint8_t >& s_bytes,
        const vector< uint8_t >& tx_hash );


    static std::vector< uint8_t > rlpEncode( const LegacyTx& tx, bool withSig,
        std::vector< uint8_t >* v_encoded, std::vector< uint8_t >* r_encoded,
        std::vector< uint8_t >* s_encoded );

    static ptr< vector< uint8_t > >  rlpEncodeWithoutSig(ParsedEthTransaction& _transaction);


    static std::vector< uint8_t > hashTransaction( const std::vector< uint8_t >& tx );

private:


    // a single context of each type can be used till the end of the program
    inline static auto getSecp256k1VerifyContext();
    inline static auto getSecp256k1SignContext();
    inline static auto getHashContext();


    inline static std::vector< uint8_t > generateRandomPrivateKey();

    inline static std::vector< uint8_t > keccak256( const std::vector< uint8_t >& data );


    inline static void rlpEncodeBytes( std::vector< uint8_t >& out, const std::vector< uint8_t >& data );


    inline static void rlpEncodeUint256( std::vector< uint8_t >& out, const std::vector< uint8_t >& value );

    inline static void rlpEncodeList(
        std::vector< uint8_t >& out, const std::vector< std::vector< uint8_t > >& elements );

    static ptr<vector<uint8_t>> signAndEncodeTx(const LegacyTx& tx);

};
