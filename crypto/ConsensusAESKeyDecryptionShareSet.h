#pragma once

#include "DecryptedAESKey.h"
#include "AESKeyDecryptionShareSet.h"


class PartialHashesList;
class Schain;
class AESKeyDecryptionShare;
class BLAKE3Hash;
class EncryptedAESKey;

class ConsensusAESKeyDecryptionShareSet : public AESKeyDecryptionShareSet {
    ptr<EncryptedAESKey> encryptedAESKey;
    libBLS::TEDecryptSet decryptionShares;  // tsafe
    recursive_mutex decryptionSharesLock;

public:
    ConsensusAESKeyDecryptionShareSet( block_id _blockId, transaction_index _transactionIndex,
        size_t _totalDecryptors, size_t _requiredDecryptors);

    ptr<DecryptedAESKey> verifyAndMergeAESKey(ptr<EncryptedAESKey> _encryptedAESKey) override;

    virtual bool addDecryptionShare(
        const ptr< AESKeyDecryptionShare >& _decryptionShare ) override;


    bool isEnough() override;

    ~ConsensusAESKeyDecryptionShareSet() override;
};
