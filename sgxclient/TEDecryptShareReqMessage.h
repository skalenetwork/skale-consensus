#pragma once
#include "sgxclient/SgxZmqMessage.h"

class TEDecryptShareReqMessage : public SgxZmqMessage {
public:
    TEDecryptShareReqMessage( shared_ptr< rapidjson::Document >& _d ) : SgxZmqMessage( _d ){};
};


