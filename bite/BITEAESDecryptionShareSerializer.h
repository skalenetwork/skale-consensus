#pragma once

#include "datastructures/CommittedBlock.h"

class AESKeyDecryptionShareList;

class BITEAESDecryptionShareSerializer {

public:

    static ptr< std::vector< uint8_t > > serialize(ptr<AESKeyDecryptionShareList> _decryptionShareList );

    static ptr< AESKeyDecryptionShareList > deserialize( const ptr< vector< uint8_t > >& _serializedDecryptionShares,
        const ptr< CryptoManager >& _manager, bool _verify);

    static void serializedSanityCheck( const ptr< vector< uint8_t > >& _serializedDecryptionShares );
};
