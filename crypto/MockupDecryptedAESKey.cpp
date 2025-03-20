#include "SkaleCommon.h"
#include "Log.h"

#include "network/Utils.h"
#include "thirdparty/json.hpp"


#include "MockupDecryptedAESKey.h"
#include "ThresholdSignature.h"


MockupDecryptedAESKey::MockupDecryptedAESKey(
    const string& _s, block_id _blockID, size_t _totalDecryptors, size_t _requiredDecryptors )
    : DecryptedAESKey( _blockID, _totalDecryptors, _requiredDecryptors ) {
    s = _s;
}


string MockupDecryptedAESKey::toString() {
    CHECK_STATE( s != "" );
    return s;
};

