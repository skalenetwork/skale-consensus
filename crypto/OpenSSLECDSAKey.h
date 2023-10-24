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

    EC_KEY* ecKey = nullptr;

    static EC_GROUP* ecgroup;
    static EC_GROUP* ecgroupFast;

    bool isFast = false;

    string ecdsaSignImpl( const char* _hash ) const;

    static EC_KEY* deserializeECDSAPubKey( const string& _publicKey );
    static EC_KEY* deserializeSGXPubKey( const string& _publicKey );

    static EC_KEY* generateECDSAKeyImpl( int nid );

    static void initGroupsIfNeeded();

public:
    OpenSSLECDSAKey( EC_KEY* _eckey, bool _isPrivate, bool _isFast );

    virtual ~OpenSSLECDSAKey();

    string serializePubKey();

    string sign( const char* hash );

    void verifySig( const string& _signature, const char* _hash );
    void verifySGXSig( const string& _sig, const char* _hash );

    static ptr< OpenSSLECDSAKey > importSGXPubKey( const string& _publicKey );
    static ptr< OpenSSLECDSAKey > importPubKey( const string& _publicKey );

    static ptr< OpenSSLECDSAKey > generateKey();
};

#endif  // OPENSSLECDSAPRIVATEKEY_H
