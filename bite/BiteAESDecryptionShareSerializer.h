#pragma once

#include "flatb/decryption_shares_generated.h"

class BiteAESDecryptionShareSerializer {
public:
    static ptr< std::vector< uint8_t > > serialize(
        ptr< AESKeyDecryptionShareList > _decryptionShareList );

    static ptr< AESKeyDecryptionShareList > deserialize(
        const ptr< vector< uint8_t > >& _serializedDecryptionShares,
        const ptr< CryptoManager >& _manager, bool _verify );

    static void serializedSanityCheck(
        const ptr< vector< uint8_t > >& _serializedDecryptionShares );

    static shared_ptr< AESKeyDecryptionShareList > getDecryptionShares( const block_id _blockId,
        const schain_index _proposerIndex, const schain_index _decryptorIndex,
        const flatbuffers::Vector< ::flatbuffers::Offset< skale_fb::DecryptionShare > >*
            _fbDecryptionSharesHandle, ptr<BiteManager> _biteManager);

private:
    static void processSingleDecryptionShare(
        const skale_fb::DecryptionShare* fbdecryptionShareHandle,
        schain_index _decryptorIndex,
        ptr<BiteManager> _biteManager,
        ptr<AESKeyDecryptionShareList> shares);
};
