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
    decryptionShares.emplace(_index, _decryptShare);
}

ptr<AESKeyDecryptionShare> AESKeyDecryptionShareList::getDecryptionShare(uint64_t id) const {
    CHECK_STATE(isComplete);
    auto it = decryptionShares.find(id);
    return (it != decryptionShares.end()) ? it->second : nullptr;
}

void AESKeyDecryptionShareList::markComplete() {
    isComplete = true;
}

string AESKeyDecryptionShareList::serializeToString() {
    // Serialize the object to a string
    return "";
}

