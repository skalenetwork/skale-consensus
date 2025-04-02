#include "Log.h"
#include "AESKeyDecryptionShareList.h"


block_id AESKeyDecryptionShareList::getBlockId() const {
    return blockId;
}

schain_index AESKeyDecryptionShareList::getDecryptorIndex() const {
    return encryptorIndex;
}


// Optional: Add public access methods
void AESKeyDecryptionShareList::addShare(transaction_index _index, const ptr<AESKeyDecryptionShare> &_decryptShare) {
    CHECK_STATE(!isComplete);
    decryptionShares.emplace(_index, _decryptShare);
}

ptr<AESKeyDecryptionShare> AESKeyDecryptionShareList::getDecryptionShare(transaction_index _transactionIndex) const {
    CHECK_STATE(isComplete);
    auto it = decryptionShares.find(_transactionIndex);
    return (it != decryptionShares.end()) ? it->second : nullptr;
}

void AESKeyDecryptionShareList::markComplete() {
    isComplete = true;
}



