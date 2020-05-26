//
// Created by kladko on 20.02.20.
//




#include <gmp.h>
#include <stdbool.h>

#include "secure_enclave/Verify.h"

#include "SkaleCommon.h"
#include "SkaleLog.h"


#include "ECDSAVerify.h"

bool ECDSAVerify::verifyECDSASig( string& pubKeyStr, ptr< string > hashHex, ptr< string > _sig ) {

    bool result = false;


    auto firstColumn = _sig->find(":");

    if (firstColumn == string::npos || firstColumn == _sig->length() - 1) {
        LOG(warn, "Misfomatted signature");
        return false;
    }

    auto secondColumn = _sig->find( ":", firstColumn + 1);

    if (secondColumn == string::npos || secondColumn == _sig->length() - 1) {
        LOG(warn, "Misformatted signature");
        return false;
    }

    auto r = _sig->substr(firstColumn + 1, secondColumn - firstColumn - 1);
    auto s = _sig->substr(secondColumn + 1, _sig->length() - secondColumn - 1);


    signature sig = signature_init();

    if (pubKeyStr.size() != 128) {

    }





    auto pubKeyR = pubKeyStr.substr(0, 64);
    auto pubKeyS = pubKeyStr.substr(64, 128);
    domain_parameters curve = domain_parameters_init();
    domain_parameters_load_curve(curve, secp256k1);
    point publicKey = point_init();


    mpz_t msgMpz;
    mpz_init(msgMpz);


    if (mpz_set_str(msgMpz, hashHex->c_str(), 16) == -1) {
        LOG(warn, "invalid message hash " +  *hashHex);
        goto clean;
    }

    if (signature_set_str(sig, r.c_str(), s.c_str(), 16) != 0) {
        LOG(warn, "Misformatted ECDSA sig ");
    }
    point_set_hex(publicKey, pubKeyR.c_str(), pubKeyS.c_str());
    if (!signature_verify(msgMpz, sig, publicKey, curve)) {
        LOG(warn, "ECDSA sig not verified");
        goto clean;
    }

    result = true;

    clean:

    mpz_clear(msgMpz);
    domain_parameters_clear(curve);
    point_clear(publicKey);
    signature_free(sig);

    return result;

}


