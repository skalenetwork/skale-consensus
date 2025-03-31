#include "SkaleCommon.h"
#include "Log.h"

#include "network/Utils.h"
#include "thirdparty/json.hpp"


#include "bls.h"
#include "threshold_encryption/TEDecryptionShare.h"

#include "ConsensusAESKeyDecryptionShare.h"

using namespace libBLS;

ConsensusAESKeyDecryptionShare::ConsensusAESKeyDecryptionShare(
    const ptr< TEDecryptionShare >& _decryptionShare, block_id _blockID,
    transaction_index _transactionIndex, schain_index _decryptorIndex, bool _decryptionFailed )
    : AESKeyDecryptionShare(_blockID, _transactionIndex, _decryptorIndex, _decryptionFailed) {
    CHECK_ARGUMENT( _decryptionShare);
    this->decryptorIndex = _decryptionShare->getSignerIndex();
    aesKeyDecryptionShare = _decryptionShare;
}



ConsensusAESKeyDecryptionShare::ConsensusAESKeyDecryptionShare( const string& _decryptionShare,
    block_id _blockID, transaction_index _transactionIndex, schain_index _decryptorIndex,
    bool _decryptionFailed, uint64_t ,
    uint64_t  )
    : AESKeyDecryptionShare( _blockID, _transactionIndex, _decryptorIndex, _decryptionFailed) {
    CHECK_ARGUMENT( _decryptionShare != "" );

    try {
        aesKeyDecryptionShare = std::make_shared< TEDecryptionShare >(
            ( uint64_t ) _decryptorIndex, _decryptionShare );
        CHECK_STATE2( aesKeyDecryptionShare->validate(), "Validation of decrypt share failed");
    } catch ( ... ) {
        throw_with_nested(
            InvalidStateException( "Could not create BLSSDecryptionShare", __CLASS_NAME__ ) );
    }
}


ptr< TEDecryptionShare > ConsensusAESKeyDecryptionShare::getTEDecryptionShare() const {
    CHECK_STATE( aesKeyDecryptionShare );
    return aesKeyDecryptionShare;
}




ConsensusAESKeyDecryptionShare::~ConsensusAESKeyDecryptionShare() {}

string ConsensusAESKeyDecryptionShare::toString() {
    try {
        auto result = getTEDecryptionShare()->toString();
        CHECK_STATE( !result.empty() );
        return result;
    } catch ( ... ) {
        throw_with_nested(
            InvalidStateException( "Could not toString() decryption share", __CLASS_NAME__ ) );
    }
}
