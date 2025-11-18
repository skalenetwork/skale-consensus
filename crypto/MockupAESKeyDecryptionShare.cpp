#include "SkaleCommon.h"
#include "Log.h"
#include "EncryptedAESKey.h"
#include "MockupAESKeyDecryptionShare.h"

MockupAESKeyDecryptionShare::MockupAESKeyDecryptionShare( const string _aesKeyDecryptionShare,
    schain_index _decryptorIndex,bool _decryptionFailed) :
AESKeyDecryptionShare(_decryptorIndex, _decryptionFailed) {
    CHECK_ARGUMENT(!_aesKeyDecryptionShare.empty());
    this->aesDecryptionShare = _aesKeyDecryptionShare;
}

MockupAESKeyDecryptionShare::~MockupAESKeyDecryptionShare() = default;

string MockupAESKeyDecryptionShare::toString() {
    CHECK_STATE(!aesDecryptionShare.empty());
    return aesDecryptionShare;
}


ptr<MockupAESKeyDecryptionShare> MockupAESKeyDecryptionShare::mockupDecrypt(EncryptedAESKey& _key,
                                                      schain_index _decryptorIndex) {
    return make_shared<MockupAESKeyDecryptionShare>(*_key.toHex(),
                                                    _decryptorIndex, false);
}
