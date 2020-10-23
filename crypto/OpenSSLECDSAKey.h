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

#ifndef OPENSSLECDSAPRIVATEKEY_H
#define OPENSSLECDSAPRIVATEKEY_H

#include "openssl/ec.h"

class OpenSSLECDSAKey {

    bool isPrivate = false;

    EC_KEY *ecKey = nullptr;
    EVP_PKEY*  edKey = nullptr;

    static EC_GROUP *ecgroup;
    static EC_GROUP *ecgroupFast;

    bool isFast;


    static EVP_PKEY* genFastKeyImpl();
    static EC_KEY* generateECDSAKeyImpl( int nid );


    static void initGroupsIfNeeded();

    string fastSignImpl( const char* _hash );

    static EVP_PKEY* deserializeFastPubKey( const string& encodedPubKeyStr );
    static EC_KEY* deserializeECDSAPubKey( const string& _publicKey );
    static EC_KEY* deserializeSGXPubKey( const string& _publicKey );

    string ecdsaSignImpl( const char* _hash) const;

public:



    OpenSSLECDSAKey( EC_KEY* _eckey,
                     EVP_PKEY*  _edKey, bool _isPrivate, bool _isFast);

    static ptr< OpenSSLECDSAKey > importSGXPubKey( const string& _publicKey);
    static ptr< OpenSSLECDSAKey > importECDSAPubKey1( const string& _publicKey);
    static ptr< OpenSSLECDSAKey > importFastPubKey1( const string& _publicKey);


    virtual ~OpenSSLECDSAKey();

    static ptr< OpenSSLECDSAKey > generateFastKey1();
    static ptr< OpenSSLECDSAKey > generateECDSAKey();

    string serializeECDSAPublicKey1();
    string serializeFastPubKey1() const;

    string signECDSA1(const char* hash);
    string signFast1(const char* hash);

    bool verifyECDSASig1(const string& _signature, const char* _hash );
    bool verifySGXSig(const string& _sig, const char* _hash);
    bool verifyFastSig1( const string& _encodedSignature, const char* _hash ) const;


};

#endif  // OPENSSLECDSAPRIVATEKEY_H
