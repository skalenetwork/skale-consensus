
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/rand.h>
#include <secp256k1.h>
#include <secp256k1_recovery.h>

#include <openssl/sha.h>
#include <boost/optional.hpp>

#include "SkaleCommon.h"
#include "Log.h"
#include "node/ConsensusInterface.h"
#include "bite/BiteCiphertext.h"
#include "bite/BiteManager.h"
#include "libBLS/threshold_encryption/ThresholdEncryption.h"
#include "crypto/EncryptedAESKey.h"
#include "ParsedEthTransaction.h"
#include "EthTransactionEncoder.h"


std::vector< uint8_t > EthTransactionEncoder::generateRandomPrivateKey() {
    std::vector< uint8_t > priv_key( 32 );

    CHECK_STATE2(RAND_bytes( priv_key.data(), priv_key.size() ) == 1, 
        "Failed to generate cryptographically secure private key" );

    // Optional: verify key is valid for secp256k1
    thread_local auto ctx = EthTransaction::getSecp256k1SignContext();
    CHECK_STATE2(secp256k1_ec_seckey_verify( ctx.get(), priv_key.data() ), 
        "Generated private key is invalid for secp256k1" );

    return priv_key;
}


ptr< vector< uint8_t > > EthTransactionEncoder::signAndEncodeTx( const EthTransaction& tx ) {


    std::vector< uint8_t > privkey = generateRandomPrivateKey();
    Signature signature = tx.sign(privkey);

    std::vector< uint8_t > encodedTx = tx.rlpEncode( std::make_optional( signature ) );
    tx.verifySignature(signature);
    return make_shared< vector< uint8_t > >( std::move( encodedTx ) );
}

/// @brief Generates a sample transaction of one of the three types (Legacy, Type1, Type2).
/// The transaction is signed and encoded. Each time it is called, it rotates through the three types.
/// @param _isByte Specifies if the transaction data should be BITE encrypted.
/// @param _biteManager 
/// @return pointer to RLP-encoded transaction
ptr< vector< uint8_t > > EthTransactionEncoder::generateSampleTx( bool _isByte, ptr<BiteManager> _biteManager ) {
    CHECK_STATE(_biteManager)
    static atomic< uint64_t > counter = 0;
    static atomic< uint64_t > nonce = 0;

    /// Transaction templates
    /// Does not need to follow the fields order - the order is only enforced when calling
    /// `encode` method
    static LegacyTx templateLegacy {
        {},                                            // nonce
        { 0x52, 0x08 },                                // gasLimit
        std::vector< uint8_t >( 20, 0x12 ),            // to
        { 0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64, 0x00 },  // value
        { 0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64, 0x00, 0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64,
            0x00 },                                    // data
        { 0xD1, 0xD2, 0xD3 },                          // chainId - will be encoded into v field of signature
        { 0x3b, 0x9a, 0xca, 0x00 },                    // gasPrice
    };

    static Type1Tx templateType1 {
        {},                                            // nonce
        { 0x52, 0x08 },                                // gasLimit
        std::vector< uint8_t >( 20, 0x12 ),            // to
        { 0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64, 0x00 },  // value
        { 0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64, 0x00, 0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64,
            0x00 },                                    // data
        { 0xD1, 0xD2, 0xD3 },                          // chainId
        { 0x3b, 0x9a, 0xca, 0x00 },                    // gasPrice
        {},                                            // accessList
    };

    static Type2Tx templateType2 {
        {},                                            // nonce
        { 0x52, 0x08 },                                // gasLimit
        std::vector< uint8_t >( 20, 0x12 ),            // to
        { 0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64, 0x00 },  // value
        { 0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64, 0x00, 0x0d, 0xe0, 0xb6, 0xb3, 0xa7, 0x64,
            0x00 },                                    // data
        { 0xD1, 0xD2, 0xD3 },                          // chainId
        { 0x3b, 0x9a, 0xca, 0x00 },                    // maxPriorityFeePerGas
        { 0x0b, 0x9a, 0xca, 0x00 },                    // maxFeePerGas
        {},                                            // accessList
    };

    // rotate over each tx type each call
    uint64_t currentTxType = counter.fetch_add( 1 ) % 3;
    TxType txType = static_cast< TxType >( currentTxType );
    auto currentNonce = nonce.fetch_add( 1 );


    // generate the tx from sample templates
    std::unique_ptr<EthTransaction> tx;
    switch ( txType ) {
        case TxType::LEGACY:
            tx = std::make_unique<LegacyTx>(templateLegacy);
            break;
        case TxType::TYPE1:
            tx = std::make_unique<Type1Tx>(templateType1);
            break;
        case TxType::TYPE2:
            tx = std::make_unique<Type2Tx>(templateType2);
            break;
        default:
            throw std::invalid_argument( "Unknown transaction type" );
    }

    EthTransaction& txRef = *tx;
    txRef.nonce = RLPStream::u256toBytes( static_cast<u256>( currentNonce ) );

    if ( _isByte ) {
        auto encryptedKeyPlusData = _biteManager->teEncryptDataAndToAddress(txRef.data, txRef.to);
        BiteCiphertext biteDataField(encryptedKeyPlusData , 0);
        // set data
        txRef.data = *biteDataField.getSerializedData();
        // set to field with BITE magic number
        txRef.to = { 0x42, 0x49, 0x54, 0x45, 0x20, 0x4D, 0x45, 0x20,
                     0x49, 0x27, 0x4D, 0x20, 0x45, 0x4E, 0x43, 0x52,
                     0x59, 0x50, 0x54, 0x44 };
    }

    auto encodedTx = signAndEncodeTx( txRef );
    CHECK_STATE( encodedTx );

    return encodedTx;
}

ptr< vector< uint8_t > >  EthTransactionEncoder::rlpEncodeWithoutSig(
    ParsedEthTransaction& _ethTransaction ) {
    auto fields = _ethTransaction.getFields();

    std::unique_ptr<EthTransaction> tx;

    auto type = _ethTransaction.getType();

    if (type >= 2) {
        throw invalid_argument( "Unknown transaction type" );
    }


    TxType txType = static_cast< TxType >( type );

    switch (txType) {
        case TxType::LEGACY:
            tx = std::make_unique<LegacyTx>(fields);
            break;
        case TxType::TYPE1:
            tx = std::make_unique<Type1Tx>(fields);
            break;
        case TxType::TYPE2:
            tx = std::make_unique<Type2Tx>(fields);
            break;
        default:
            throw invalid_argument( "Unknown transaction type" );
    }

    // encode without signature
    auto result =  tx->rlpEncode( nullopt );

    return make_shared<vector< uint8_t >>(result);
}
