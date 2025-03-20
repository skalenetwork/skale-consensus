#pragma once

#include "sgxclient/SgxZmqMessage.h"

class TEDecryptShareRspMessage : public SgxZmqMessage {
public:

    TEDecryptShareRspMessage( shared_ptr< rapidjson::Document >& _d ) : SgxZmqMessage( _d ){};

    string getTEAES256KeyDecryptShare() {
        //return getStringRapid( "signatureShare" );
        //TODOBITE
        return ""; // This should return hex encoded AES key decryption share
        // TODOBITE END
    }
};
