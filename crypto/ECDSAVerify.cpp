//
// Created by kladko on 20.02.20.
//




#include <gmp.h>
#include <stdbool.h>

#include "secure_enclave/Verify.h"

#include "SkaleCommon.h"
#include "SkaleLog.h"


#include "ECDSAVerify.h"

bool ECDSAVerify::verifyECDSASig(string& pubKeyStr, const char *hashHex, const char *signatureR,
                    const char *signatureS) {

    bool result = false;

    signature sig = signature_init();

    auto r = pubKeyStr.substr(0, 64);
    auto s = pubKeyStr.substr(64, 128);
    domain_parameters curve = domain_parameters_init();
    domain_parameters_load_curve(curve, secp256k1);
    point publicKey = point_init();


    mpz_t msgMpz;
    mpz_init(msgMpz);


    if (mpz_set_str(msgMpz, hashHex, 16) == -1) {
        spdlog::error("invalid message hash {}", hashHex);
        goto clean;
    }

    signature_set_str(sig, signatureR, signatureS, 16);

    point_set_hex(publicKey, r.c_str(), s.c_str());
    if (!signature_verify(msgMpz, sig, publicKey, curve)) {
        spdlog::error("ECDSA sig not verified");
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


