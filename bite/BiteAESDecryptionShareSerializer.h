#pragma once

#include "flatb/decryption_shares_generated.h"
#include "bite/crypto/CryptographicValidationMode.h"

class BiteAESDecryptionShareSerializer {
public:
    

    static ptr< std::vector< uint8_t > > serialize(
        ptr< AESKeyDecryptionShareList > _decryptionShareList );

    static ptr< AESKeyDecryptionShareList > deserialize(
        const ptr< vector< uint8_t > >& _serializedDecryptionShares,
        const ptr< CryptoManager >& _manager, CryptographicValidationMode _validationMode = CryptographicValidationMode::Validate );

    static void serializedSanityCheck(
        const ptr< vector< uint8_t > >& _serializedDecryptionShares );

    /**
     * @brief Creates an AESKeyDecryptionShareList from the given FlatBuffer decryption shares.
     * @param _validationMode Cryptographic validation policy for reconstructed shares:
     *        use Validate for any payload received from external nodes; use
     *        SkipValidationTrustedSource only when reloading this node's own
     *        previously persisted shares from local DB.
     */
    static shared_ptr< AESKeyDecryptionShareList > getDecryptionShares( const block_id _blockId,
        const schain_index _proposerIndex, const schain_index _decryptorIndex,
        const flatbuffers::Vector< ::flatbuffers::Offset< skale_fb::DecryptionShare > >*
            _fbDecryptionSharesHandle, ptr<BiteManager> _biteManager, 
            CryptographicValidationMode _validationMode = CryptographicValidationMode::Validate );
};
