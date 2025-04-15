#pragma once

class ParsedEthTransaction;
class BiteManager;


class EthTransactionEncoder {

    using uint256 = std::vector< uint8_t >;


public:

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

    /**
     * @brief Base transaction fields - common to all transactions.
     * Fields defined in this struct do not follow RLP-encoded order.
     * This order should be enforced in the `encode` implementation for each
     * type of transaction
     */
    struct Transaction {
        uint256 nonce;
        uint256 gasLimit;
        std::vector< uint8_t > to;  // 20 bytes
        uint256 value;
        std::vector< uint8_t > data;
        // not included in RLP-encoding for legacy tx
        uint256 chainId;

        Transaction(
            const uint256& nonce,
            const uint256& gasLimit,
            const std::vector<uint8_t>& to,
            const uint256& value,
            const std::vector<uint8_t>& data,
            const uint256& chainId)
            : nonce(nonce), gasLimit(gasLimit), to(to), value(value), data(data), chainId(chainId)
        {}

        /**
         * @brief Encode the object into RLP format. Return a vector of each encoded field.
         * Field order is enforced by this function
         */
        virtual std::vector< std::vector< uint8_t > > encode() const = 0;

        /**
         * @brief Returns a valid prefix representing the tx type.
         * Returns -1 if the transaction type does not include prefix
         */
        virtual TxPrefix getBytePrefix() const = 0;
    };

    struct LegacyTx : Transaction {
        uint256 gasPrice;

        LegacyTx(
            const uint256& nonce,
            const uint256& gasLimit,
            const std::vector<uint8_t>& to,
            const uint256& value,
            const std::vector<uint8_t>& data,
            const uint256& chainId,
            const uint256& _gasPrice)
        : Transaction(nonce, gasLimit, to, value, data, chainId),
        gasPrice(_gasPrice)
        {}

        LegacyTx(std::vector< std::vector< uint8_t > >& fields) :
            Transaction(
                fields.at( 0 ), // nonce
                fields.at( 2 ), // gasLimit
                fields.at( 3 ), // to
                fields.at( 4 ), // value
                fields.at( 5 ), // data
                {} // chainId
            ),
            gasPrice(fields.at( 1 ))
        {}

        std::vector< std::vector< uint8_t > > encode() const override;
        TxPrefix getBytePrefix() const override;
    };

    /**
     * @brief Access tuple for EIP-2930 transactions
     */
    struct AccessTuple {
        std::vector<uint8_t> address;                  // 20 bytes
        std::vector<std::vector<uint8_t>> storageKeys; // 32-byte keys

        std::vector< uint8_t > encode() const;
    };

    /**
     *  @brief EIP-2930 transaction fields - does not need to follow the same
     *  order - the order will be enforced in the `encode` method
     */
    struct Type1Tx : Transaction {
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
            : Transaction(nonce, gasLimit, to, value, data, chainId),
            gasPrice(_gasPrice), accessList(_accessList)
        {}

        Type1Tx(std::vector< std::vector< uint8_t > >& fields) :
            Transaction(
                fields.at( 1 ), // nonce
                fields.at( 3 ), // gasLimit
                fields.at( 4 ), // to
                fields.at( 5 ), // value
                fields.at( 6 ), // data
                fields.at( 0 )  // chainId
            ),
            gasPrice(fields.at( 2 )),
            accessList({})
        {}

        std::vector< std::vector< uint8_t > > encode() const override;
        TxPrefix getBytePrefix() const override;
    };

    /**
     *  @brief EIP-1559 transaction fields - does not need to follow the same
     *  order - the order will be enforced in the `encode` method
     */
    struct Type2Tx : Transaction {
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
            : Transaction(nonce, gasLimit, to, value, data, chainId),
            maxPriorityFeePerGas(_maxPriorityFeePerGas), maxFeePerGas(_maxFeePerGas), accessList(_accessList)
        {}

        Type2Tx(std::vector< std::vector< uint8_t > >& fields) :
        Transaction(
            fields.at( 1 ), // nonce
            fields.at( 4 ), // gasLimit
            fields.at( 5 ), // to
            fields.at( 6 ), // value
            fields.at( 7 ), // data
            fields.at( 0 ) // chainId
            ),
            maxPriorityFeePerGas(fields.at( 2 )),
            maxFeePerGas(fields.at( 3 )),
            accessList({})
        {}

        std::vector< std::vector< uint8_t > > encode() const override;
        TxPrefix getBytePrefix() const override;
    };


    static std::shared_ptr<std::vector<uint8_t>> generateSampleTx(bool _isByte, ptr<BiteManager> _biteManager);

    static void uint64toVec( uint64_t v_value, std::vector< uint8_t >& v_vec );

    static void verifyEthSignature( const std::vector< uint8_t >& v_vec,
        const std::vector< uint8_t >& r_bytes, const std::vector< uint8_t >& s_bytes,
        const std::vector< uint8_t >& tx_hash );


    static std::vector< uint8_t > rlpEncode( const Transaction& tx, bool withSig,
        std::vector< uint8_t >* v_encoded, std::vector< uint8_t >* r_encoded,
        std::vector< uint8_t >* s_encoded );

    static std::shared_ptr< std::vector< uint8_t > >  rlpEncodeWithoutSig(ParsedEthTransaction& _transaction);


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

    static std::shared_ptr<std::vector<uint8_t>> signAndEncodeTx(const Transaction& tx);


    inline static void addEncodedFieldUint256(std::vector< std::vector< uint8_t > >& fields, const uint256& val);

    inline static void addEncodedFieldBytes(std::vector< std::vector< uint8_t > >& fields, const std::vector< uint8_t >& val);

    inline static void addEncodedFieldList(std::vector< std::vector< uint8_t > >& fields, const std::vector< std::vector< uint8_t > >& val);

};
