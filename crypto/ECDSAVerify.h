//
// Created by kladko on 20.02.20.
//

#ifndef SKALED_ECDSAVERIFY_H
#define SKALED_ECDSAVERIFY_H

class ECDSAVerify {




public:

    static bool verifyECDSASig(string& pubKeyStr, const char *hashHex, const char *signatureR,
                        const char *signatureS);
};


#endif //SKALED_ECDSAVERIFY_H
