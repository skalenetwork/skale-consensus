/*
    Copyright (C) 2019 SKALE Labs

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

    @file SerializationTests.cpp
    @author Stan Kladko
    @date 2019
*/



#include "../thirdparty/catch.hpp"


#include "openssl/bio.h"

#include "openssl/evp.h"
#include "openssl/rsa.h"
#include "openssl/pem.h"


#include "../SkaleCommon.h"
#include "../exceptions/ParsingException.h"

#define BOOST_PENDING_INTEGER_LOG2_HPP



class CryptoFixture {
public:
    CryptoFixture() {
    };

    ~CryptoFixture(){
    }
};


TEST_CASE_METHOD(CryptoFixture, "Import pem ecdsa key", "[import-ecdsa-key]") {

    //openssl ecparam -name secp256k1 -genkey -noout

    const char* insecureECDSAKey =
            "-----BEGIN EC PRIVATE KEY-----"
            "MHQCAQEEINbmHz6w9lvoNvgPPRwkVSJVAD0zS3Rhd2YMQl6fcLpFoAcGBSuBBAAK"
            "oUQDQgAEmtFhQ0RnjT1zQYhYUcKAi5j1E6wAu5dAo9pileYW0fgDX2533s1FUSiz"
            "Mg90hwa2Z50fcIxS9JY8SFuf+tllyQ=="
            "-----END EC PRIVATE KEY-----";

    BIO *bio;

    EVP_PKEY* result;

    bio = BIO_new_mem_buf((void *)insecureECDSAKey, strlen(insecureECDSAKey));
    result = PEM_read_bio_PrivateKey(
            bio,   /* BIO to read the private key from */
            NULL, /* pointer to EVP_PKEY structure */
            NULL,  /* password callback - can be NULL */
            NULL   /* parameter passed to callback or password if callback is NULL */
    );

    REQUIRE(result != nullptr);

}
