#include "SkaleCommon.h"
#include "Log.h"

#include "network/Utils.h"
#include "thirdparty/json.hpp"


#include "MockupDecryptedAES256Key.h"
#include "ThresholdSignature.h"


MockupDecryptedAES256Key::MockupDecryptedAES256Key(
    const string& _s, block_id _blockID, size_t _totalDecryptors, size_t _requiredDecryptors )
    : DecryptedAES256Key( _blockID, _totalDecryptors, _requiredDecryptors ) {
    s = _s;
}


string MockupDecryptedAES256Key::toString() {
    CHECK_STATE( s != "" );
    return s;
};

