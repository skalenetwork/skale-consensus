/*
    Copyright (C) 2020-present SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file OpenSSLECDSAPrivateKey.cpp
    @author Stan Kladko
    @date 2020
*/

#include <openssl/ec.h>  // for EC_GROUP_new_by_curve_name, EC_GROUP_free, EC_KEY_new, EC_KEY_set_group, EC_KEY_generate_key, EC_KEY_free
#include <openssl/ecdsa.h>    // for ECDSA_do_sign, ECDSA_do_verify
#include <openssl/obj_mac.h>  // for NID_secp192k1


#include "Log.h"

#include "network/Utils.h"
#include "OpenSSLECDSAPrivateKey.h"

#include "SkaleCommon.h"
OpenSSLECDSAPrivateKey::OpenSSLECDSAPrivateKey( EC_KEY* _ecKey ) {
    CHECK_STATE( ecKey );
    this->ecKey = _ecKey;
}
OpenSSLECDSAPrivateKey::~OpenSSLECDSAPrivateKey() {
    if ( ecKey )
        EC_KEY_free( ecKey );
}
ptr< OpenSSLECDSAPrivateKey > OpenSSLECDSAPrivateKey::generateKey() {
    EC_KEY* eckey = EC_KEY_new();
    CHECK_STATE( eckey );


    if ( ecgroup == nullptr ) {
        ecgroup = EC_GROUP_new_by_curve_name( NID_secp192k1 );
        CHECK_STATE( ecgroup );
    }

    CHECK_STATE(EC_KEY_set_group(eckey,ecgroup) == 1);

    CHECK_STATE(EC_KEY_generate_key(eckey) == 1)

    return make_shared< OpenSSLECDSAPrivateKey >( eckey );
}


EC_GROUP* OpenSSLECDSAPrivateKey::ecgroup = nullptr;

EC_KEY* OpenSSLECDSAPrivateKey::getEcKey() const {
    CHECK_STATE(ecKey);
    return ecKey;
}
ptr< string > OpenSSLECDSAPrivateKey::signHash( const char* _hash ) {

    CHECK_ARGUMENT(_hash);
    CHECK_STATE(ecKey);

    ECDSA_SIG* signature = ECDSA_do_sign( ( const unsigned char* ) _hash, 32, ecKey );

    CHECK_STATE(signature);

    uint64_t  sigLen = i2d_ECDSA_SIG(signature, nullptr);

    vector<unsigned char> sigDer(sigLen, 0);

    auto pointer = sigDer.data();

    CHECK_STATE(i2d_ECDSA_SIG(signature, &(pointer)) > 0);

    auto hexSig = Utils::carray2Hex(sigDer.data(), sigLen);

    CHECK_STATE(hexSig);

    return hexSig;
}
