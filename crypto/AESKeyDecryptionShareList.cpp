#include "Log.h"
#include "AESKeyDecryptionShareList.h"


block_id AESKeyDecryptionShareList::getBlockId() const {
    return blockId;
}

schain_index AESKeyDecryptionShareList::getDecryptorIndex() const {
    return decryptorIndex;
}

void AESKeyDecryptionShareList::reserve(size_t _size) {
    decryptionShares.reserve( _size );
}

// Optional: Add public access methods
void AESKeyDecryptionShareList::addShares(transaction_index _index, const ptr<AESKeyDecryptionShares> &_decryptShares) {
    decryptionShares.emplace(_index, _decryptShares);
    totalCiphertextShares += _decryptShares->size();
}

ptr<AESKeyDecryptionShares> AESKeyDecryptionShareList::getDecryptionShares(transaction_index _transactionIndex) const {
    auto it = decryptionShares.find(_transactionIndex);
    return (it != decryptionShares.end()) ? it->second : nullptr;
}




