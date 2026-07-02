
#include "SkaleCommon.h"
#include "Log.h"


#include "DecryptedAESKeyList.h"

const boost::container::flat_map<transaction_index, ptr<DecryptedAESKeys>>& DecryptedAESKeyList::getKeys() const {
    return decryptedAESKeys;
}

boost::container::flat_map<transaction_index, ptr<DecryptedAESKeys>>& DecryptedAESKeyList::getKeys() {
    return decryptedAESKeys;
}
