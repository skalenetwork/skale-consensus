#pragma once

#include "datastructures/DataStructure.h"
#include "AESKeyDecryptionShareSet.h"


class PartialHashesList;
class Schain;
class MockupAESKeyDecryptionShare;
class AESKeyDecryptionShare;
class DecryptedAESKey;
class BLAKE3Hash;

class MockupAESKeyDecryptionShareSet : public AESKeyDecryptionShareSet {
    std::map< size_t, ptr< MockupAESKeyDecryptionShare > > decryptionShares;  // tsafe
    shared_mutex decryptionSharesLock;

    bool isEnoughUnsafe();

public:
    MockupAESKeyDecryptionShareSet( block_id _blockId, transaction_index _transactionIndex,
        size_t _totalDecryptors, size_t _requiredDecryptors );

    ptr< DecryptedAESKey > verifyAndMergeAESKey();

    bool addDecryptionShare( const ptr< AESKeyDecryptionShare >& _sigShare );

    bool isEnough();

    virtual ~MockupAESKeyDecryptionShareSet();
};
