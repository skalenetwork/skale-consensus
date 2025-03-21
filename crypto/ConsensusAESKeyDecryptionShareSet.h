#pragma once


#include "ThresholdAESKeyDecryptionShareSet.h"


class PartialHashesList;
class Schain;
class ThresholdAESKeyDecryptionShare;
class BLAKE3Hash;

class ConsensusAESKeyDecryptionShareSet : public ThresholdAESKeyDecryptionShareSet {
    std::map< size_t, ptr< ConsensusAESKeyDecryptionShare > > decryptionShares;  // tsafe
    recursive_mutex decryptionSharesLock;

public:
    ConsensusAESKeyDecryptionShareSet( block_id _blockId, transaction_index _transactionIndex,
        size_t _totalDecryptors, size_t _requiredDecryptors );

    ptr< DecryptedAESKey > mergeAESKey() override;

    virtual bool addDecryptionShare(
        const ptr< ThresholdAESKeyDecryptionShare >& _decryptionShare ) override;


    bool isEnough() override;

    ~ConsensusAESKeyDecryptionShareSet() override;
};
