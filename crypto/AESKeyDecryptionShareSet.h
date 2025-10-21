#pragma once
#include <datastructures/BlockProposal.h>

class EncryptedAESKey;
class AESKeyDecryptionShare;
class DecryptedAESKey;

class AESKeyDecryptionShareSet {
public:
    AESKeyDecryptionShareSet( const block_id _blockId, transaction_index _transactionIndex );

protected:
    block_id blockId = 0;
    transaction_index transactionIndex = 0;

    static atomic< int64_t > totalObjects;

public:
    virtual ~AESKeyDecryptionShareSet();

    static int64_t getTotalObjects() {
        return totalObjects;
    }

    virtual ptr< DecryptedAESKey > verifyAndMergeAESKey(ptr<EncryptedAESKey> _encryptedAesKey) = 0;

    virtual bool isEnough() = 0;

    virtual bool addDecryptionShare( const ptr< AESKeyDecryptionShare >& _decryptionShare ) = 0;
};



