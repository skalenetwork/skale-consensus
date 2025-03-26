#include "SkaleCommon.h"
#include "Log.h"
#include "EncryptedAESKey.h"
#include "MockupAESKeyDecryptionShare.h"

MockupAESKeyDecryptionShare::MockupAESKeyDecryptionShare(const string _aesKeyDecryptionShare,
                                                         schain_id _schainID, block_id _blockID,
                                                         transaction_index _transactionIndex,
                                                         schain_index _decryptorIndex, size_t _totalDecryptors,
                                                         size_t _requiredDecryptors)
    : ThresholdAESKeyDecryptionShare(_schainID, _blockID, _transactionIndex, _decryptorIndex) {
    CHECK_ARGUMENT(!_aesKeyDecryptionShare.empty());
    CHECK_ARGUMENT(_requiredDecryptors <= _totalDecryptors);
    this->totalDecryptors = _totalDecryptors;
    this->requiredDecryptors = _requiredDecryptors;
    this->aesDecryptionShare = _aesKeyDecryptionShare;
}

MockupAESKeyDecryptionShare::~MockupAESKeyDecryptionShare() = default;

string MockupAESKeyDecryptionShare::toString() {
    CHECK_STATE(!aesDecryptionShare.empty());
    return aesDecryptionShare;
}


static ptr<MockupAESKeyDecryptionShare> mockupDecrypt(ptr<EncryptedAESKey> _key,
                                                      schain_id _schainID, block_id _blockID,
                                                      transaction_index _transactionIndex,
                                                      schain_index _decryptorIndex, size_t _totalDecryptors,
                                                      size_t _requiredDecryptors) {
    return make_shared<MockupAESKeyDecryptionShare>(*_key->toHex(), _schainID, _blockID, _transactionIndex,
                                                    _decryptorIndex, _totalDecryptors, _requiredDecryptors);
}
