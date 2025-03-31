
#include "SkaleCommon.h"
#include "BLAKE3Hash.h"
#include "AESKeyDecryptionShare.h"


AESKeyDecryptionShare::AESKeyDecryptionShare(const block_id& _blockId,
    const transaction_index& _transactionIndex, const schain_index& _decryptorIndex, bool _decryptionFailed )
    : blockId( _blockId ), transactionIndex(_transactionIndex),
      decryptorIndex( _decryptorIndex ), decryptionFailed(_decryptionFailed) {}



AESKeyDecryptionShare::~AESKeyDecryptionShare() {}




BLAKE3Hash AESKeyDecryptionShare::computeHash() {
    auto str = this->toString();
    auto v = make_shared< vector< uint8_t > >( str.size() );
    copy( str.begin(), str.end(), v->begin() );
    return BLAKE3Hash::calculateHash( v );
}
