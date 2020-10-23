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

    @file OpenSSLECDSAPrivateKey.h
    @author Stan Kladko
    @date 2020
*/

#ifndef OPENSSLEDDSAPRIVATEKEY_H
#define OPENSSLEDDSAPRIVATEKEY_H

#include "openssl/ec.h"

class OpenSSLEdDSAKey {

    bool isPrivate = false;

    EVP_PKEY*  edKey = nullptr;


    static EVP_PKEY* genFastKeyImpl();


    string fastSignImpl( const char* _hash );

    static EVP_PKEY* deserializeFastPubKey( const string& encodedPubKeyStr );



public:


    OpenSSLEdDSAKey( EVP_PKEY* _edKey, bool _isPrivate);

    static ptr< OpenSSLEdDSAKey > importFastPubKey( const string& _publicKey);

    virtual ~OpenSSLEdDSAKey();

    static ptr< OpenSSLEdDSAKey > generateFastKey();


    string serializeFastPubKey() const;

    string signFast(const char* hash);

    bool verifyFastSig( const string& _encodedSignature, const char* _hash ) const;

};

#endif  // OPENSSLECDSAPRIVATEKEY_H
