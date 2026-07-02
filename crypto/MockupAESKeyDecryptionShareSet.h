#pragma once

#include <datastructures/BlockProposal.h>

#include "datastructures/DataStructure.h"
#include "AESKeyDecryptionShareSet.h"


class PartialHashesList;
class Schain;
class MockupAESKeyDecryptionShare;
class AESKeyDecryptionShare;
class DecryptedAESKey;
class BLAKE3Hash;

class MockupAESKeyDecryptionShareSet : public AESKeyDecryptionShareSet {

    // stores single share from each decryptor
    std::map< size_t, ptr< MockupAESKeyDecryptionShare > > decryptionShares;  // tsafe
    
    size_t totalDecryptors; 
    size_t requiredDecryptors;
    
    shared_mutex decryptionSharesLock;

    bool isEnoughUnsafe();

public:
    MockupAESKeyDecryptionShareSet( block_id _blockId, transaction_index _transactionIndex,
        size_t _totalDecryptors, size_t _requiredDecryptors );

    ptr< DecryptedAESKeys > verifyAndMergeAESKeys(EncryptedAESKeys&  _encryptedAESKey) override;

    bool addDecryptionSharesFromSameDecryptor( const ptr< AESKeyDecryptionShares >& _sigShare ) override;

    bool isEnough() override;

    virtual ~MockupAESKeyDecryptionShareSet();
};
