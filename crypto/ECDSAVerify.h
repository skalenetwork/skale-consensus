//
// Created by kladko on 20.02.20.
//

#ifndef SKALED_ECDSAVERIFY_H
#define SKALED_ECDSAVERIFY_H

class ECDSAVerify {




public:

    static bool verifyECDSASig( string& pubKeyStr, ptr< string > hashHex, ptr< string > _sig );
};


#endif //SKALED_ECDSAVERIFY_H
