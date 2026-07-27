#include "SkaleCommon.h"
#include "Log.h"

#include "network/Utils.h"
#include "thirdparty/json.hpp"


#include "bls.h"
#include "threshold_encryption/TEDecryptionShare.h"

#include "ConsensusAESKeyDecryptionShare.h"

#ifdef BITE

using namespace libBLS;

ConsensusAESKeyDecryptionShare::ConsensusAESKeyDecryptionShare(
    const ptr< TEDecryptionShare >& _decryptionShare, schain_index _decryptorIndex,
    bool _decryptionFailed )
    : AESKeyDecryptionShare( _decryptorIndex, _decryptionFailed ) {
    CHECK_ARGUMENT( _decryptionShare );
    this->decryptorIndex = _decryptionShare->getSignerIndex();
    aesKeyDecryptionShare = _decryptionShare;
}


ConsensusAESKeyDecryptionShare::ConsensusAESKeyDecryptionShare(
    const string& _decryptionShare, schain_index _decryptorIndex, bool _decryptionFailed, 
    CryptographicValidationMode _validationMode )
    : AESKeyDecryptionShare( _decryptorIndex, _decryptionFailed ) {
    CHECK_ARGUMENT( _decryptionShare != "" );

    try {
        bool shouldValidate = shouldValidateAsBool(_validationMode);
        // validate() is called in TEDecryptionShare constructor
        aesKeyDecryptionShare =
            std::make_shared< TEDecryptionShare >( _decryptionShare, ( uint64_t ) _decryptorIndex, shouldValidate );
    } CATCH_LOG_AND_RETHROW_ANY_EXCEPTION( err, "Could not create TEDecryptionShare" );
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
#endif