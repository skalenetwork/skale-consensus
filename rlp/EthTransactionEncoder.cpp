
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


ptr< vector< uint8_t > > EthTransactionEncoder::signAndEncodeTx( const std::unique_ptr<EthTransaction>& tx ) {
    std::vector< uint8_t > privkey = generateRandomPrivateKey();
    Signature signature = tx->sign(privkey);

    std::vector< uint8_t > encodedTx = tx->rlpEncode( std::make_optional( signature ) );
    tx->verifySignature(signature);
    return make_shared< vector< uint8_t > >( std::move( encodedTx ) );
}


std::unique_ptr<EthTransaction> EthTransactionEncoder::generateSampleTx() {
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
    tx->nonce = RLPStream::u256toBytes( static_cast<u256>( currentNonce ) );
    return tx;
}


void EthTransactionEncoder::encryptRegularTransaction(std::unique_ptr<EthTransaction>& tx, std::shared_ptr<BiteManager> _biteManager) {
    uint64_t epochId = 0;
    auto encryptedKeyPlusData = _biteManager->encryptRegularTx(tx->data, tx->to, epochId);
    // set data
    tx->data = *encryptedKeyPlusData;
    // set to field with BITE magic number
    tx->to = { 0x42, 0x49, 0x54, 0x45, 0x20, 0x4D, 0x45, 0x20,
                0x49, 0x27, 0x4D, 0x20, 0x45, 0x4E, 0x43, 0x52,
                0x59, 0x50, 0x54, 0x44 };
}


#ifdef BITE

void EthTransactionEncoder::encryptCTXTransaction(std::unique_ptr<EthTransaction>& tx, std::shared_ptr<BiteManager> _biteManager) {
    uint64_t epochId = 0;
    
    std::optional<AddressBytes> scAddressAadTE = std::nullopt;
    if (tx->to.size() == 20) {
        AddressBytes addr;
        std::copy(tx->to.begin(), tx->to.end(), addr.begin());
        scAddressAadTE = addr;
    }

    auto catData = _biteManager->generateEncryptedCTXData(epochId, scAddressAadTE);
    tx->data = *catData;
}

void EthTransactionEncoder::encryptEmptyCTXTransaction(std::unique_ptr<EthTransaction>& tx, std::shared_ptr<BiteManager> _biteManager) {
    uint64_t epochId = 0;
    // Empty CAT also has SC address AAD if needed (though empty CATs usually don't have encrypted args needing AAD validation on decryption of args, 
    // but BiteEngine::buildCTXData might be used if we change logic. 
    // Currently generateEmptyCTXData doesn't call buildCTXData with encryption, so maybe no change needed there?)
    // Let's check generateEmptyCTXData implementation.
    auto catData = _biteManager->generateEmptyCTXData(epochId);
    tx->data = *catData;
}

#endif

std::shared_ptr< std::vector< uint8_t > >  EthTransactionEncoder::rlpEncodeWithoutSig(
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
