
#include "SkaleCommon.h"
#include "Log.h"


#include "DecryptedAESKeyList.h"

boost::container::flat_map<transaction_index, ptr<DecryptedAESKeys>>& DecryptedAESKeyList::getKeys()  {
    return decryptedAESKeys;
}
