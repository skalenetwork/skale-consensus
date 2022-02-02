//
// Created by kladko on 01.02.22.
//

#include "SkaleCommon.h"
#include "Log.h"

#include "BlockDecryptedArguments.h"

BlockDecryptedArguments::BlockDecryptedArguments() {
    decryptedArgs = make_shared<map<uint64_t, ptr<vector<uint8_t>>>>();
}
