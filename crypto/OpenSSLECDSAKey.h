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


class OpenSSLECDSAKey {

    bool isPrivate = false;

    EC_KEY *ecKey = nullptr;
    EVP_PKEY*  edKey = nullptr;

    static EC_GROUP *ecgroup;
    static EC_GROUP *ecgroupFast;

    bool isFast;


    static EVP_PKEY* genFastKeyImpl();
    static EC_KEY* generateECDSAKeyImpl( int nid );



public:

    OpenSSLECDSAKey( EC_KEY* _eckey,
                     EVP_PKEY*  _edKey, bool _isPrivate, bool _isFast);

    static ptr< OpenSSLECDSAKey > importSGXPubKey( const string& _publicKey);
    static ptr< OpenSSLECDSAKey > importFastPubKey( const string& _publicKey);

    virtual ~OpenSSLECDSAKey();

    static ptr< OpenSSLECDSAKey > generateFastKey();

    string serializeECDSAPublicKey();
    string serializeFastPubKey() const;

    string sessionSign(const char* hash);
    bool sessionVerifySig(const string& _signature, const char* _hash );
    bool verifySGXSig(const string& _sig, const char* _hash);

    static void initGroupsIfNeeded();


    string fastSignImpl( const char* _hash );
    bool verifyFastSig( const char* _hash, const string& _encodedSignature ) const;

    EVP_PKEY* decodePubKey( string& encodedPubKeyStr ) const;
};

#endif  // OPENSSLECDSAPRIVATEKEY_H
