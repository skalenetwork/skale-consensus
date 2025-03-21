#pragma once

#include "datastructures/DataStructure.h"
#include "ThresholdAESKeyDecryptionShareSet.h"


class PartialHashesList;
class Schain;
class MockupAESKeyDecryptionShare;
class ThresholdAESKeyDecryptionShare;
class DecryptedAESKey;
class BLAKE3Hash;

class MockupAESKeyDecryptionShareSet : public ThresholdAESKeyDecryptionShareSet {
    std::map< size_t, ptr< MockupAESKeyDecryptionShare > > decryptionShares;  // tsafe
    recursive_mutex decryptionSharesLock;

public:
    MockupAESKeyDecryptionShareSet( block_id _blockId, transaction_index _transactionIndex,
        size_t _totalDecryptors, size_t _requiredDecryptors );

    ptr< DecryptedAESKey > mergeAESKey();

    bool addDecryptionShare( const ptr< ThresholdAESKeyDecryptionShare >& _sigShare );

    bool isEnough();

    virtual ~MockupAESKeyDecryptionShareSet();
};
