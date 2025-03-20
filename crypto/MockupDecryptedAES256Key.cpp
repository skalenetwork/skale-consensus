#include "SkaleCommon.h"
#include "Log.h"

#include "network/Utils.h"
#include "thirdparty/json.hpp"


#include "MockupDecryptedAES256Key.h"
#include "ThresholdSignature.h"


MockupDecryptedAES256Key::MockupDecryptedAES256Key(
    const string& _s, block_id _blockID, size_t _totalNodes, size_t _requiredNodes )
    : DecryptedAES256Key( _blockID, _totalNodes, _requiredNodes ) {
    s = _s;
}


string MockupDecryptedAES256Key::toString() {
    CHECK_STATE( s != "" );
    return s;
};

