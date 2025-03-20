
#include "SkaleCommon.h"
#include "BLAKE3Hash.h"
#include "ThresholdAESKeyDecryptionShare.h"


ThresholdAESKeyDecryptionShare::ThresholdAESKeyDecryptionShare(
    const schain_id& _schainId, const block_id& _blockId,
    const transaction_index& _transactionIndex, const schain_index& _decryptorIndex )
    : schainId( _schainId ), blockId( _blockId ), transactionIndex(_transactionIndex),
      decryptorIndex( _decryptorIndex ) {}

block_id ThresholdAESKeyDecryptionShare::getBlockId() const {
    return blockId;
}

transaction_index ThresholdAESKeyDecryptionShare::getTransactionIndex() const {
    return transactionIndex;
}


ThresholdAESKeyDecryptionShare::~ThresholdAESKeyDecryptionShare() {}

schain_index ThresholdAESKeyDecryptionShare::getDecryptorIndex() const {
    return decryptorIndex;
}


BLAKE3Hash ThresholdAESKeyDecryptionShare::computeHash() {
    auto str = this->toString();
    auto v = make_shared< vector< uint8_t > >( str.size() );
    copy( str.begin(), str.end(), v->begin() );
    return BLAKE3Hash::calculateHash( v );
}
