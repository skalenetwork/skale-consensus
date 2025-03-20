
#include "SkaleCommon.h"
#include "BLAKE3Hash.h"
#include "ThresholdAES256KeyDecryptionShare.h"


ThresholdAES256KeyDecryptionShare::ThresholdAES256KeyDecryptionShare(
    const schain_id& _schainId, const block_id& _blockId,
    const transaction_index& _transactionIndex, const schain_index& _decryptorIndex )
    : schainId( _schainId ), blockId( _blockId ), transactionIndex(_transactionIndex),
      decryptorIndex( _decryptorIndex ) {}

block_id ThresholdAES256KeyDecryptionShare::getBlockId() const {
    return blockId;
}

transaction_index ThresholdAES256KeyDecryptionShare::getTransactionIndex() const {
    return transactionIndex;
}


ThresholdAES256KeyDecryptionShare::~ThresholdAES256KeyDecryptionShare() {}

schain_index ThresholdAES256KeyDecryptionShare::getDecryptorIndex() const {
    return decryptorIndex;
}


BLAKE3Hash ThresholdAES256KeyDecryptionShare::computeHash() {
    auto str = this->toString();
    auto v = make_shared< vector< uint8_t > >( str.size() );
    copy( str.begin(), str.end(), v->begin() );
    return BLAKE3Hash::calculateHash( v );
}
