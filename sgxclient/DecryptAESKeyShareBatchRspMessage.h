#pragma once

#include "sgxclient/SgxZmqMessage.h"

class DecryptAESKeyShareBatchRspMessage : public SgxZmqMessage {
public:

    DecryptAESKeyShareBatchRspMessage( shared_ptr< rapidjson::Document >& _d ) : SgxZmqMessage( _d ){};

    ptr<vector<string>> getAEKeyDecryptShares() {
        return getStringArrayRapid( "decryptionShares" );
    }
};
