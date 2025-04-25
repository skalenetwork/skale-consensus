#pragma once

#include "sgxclient/SgxZmqMessage.h"

class DecryptAESKeyShareBatchRspMessage : public SgxZmqMessage {
public:

    DecryptAESKeyShareBatchRspMessage( shared_ptr< rapidjson::Document >& _d ) : SgxZmqMessage( _d ){};

    ptr<vector<ptr<string>>> getAEKeyDecryptShares() {
        return getStringArrayRapid( "signatureShare" );
    }
};
