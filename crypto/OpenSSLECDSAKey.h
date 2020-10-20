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

    static EC_GROUP *ecgroup;


public:

    OpenSSLECDSAKey( EC_KEY* _eckey, bool _isPrivate );

    static ptr<OpenSSLECDSAKey> makeKey(const ptr<string>& _publicKey, bool _isSGXKey );

    virtual ~OpenSSLECDSAKey();

    static ptr< OpenSSLECDSAKey > generateKey();

    ptr<string> getPublicKey();

    ptr<string> sessionSign(const char* hash);

    bool sessionVerifySig(const ptr<string>& _signature, const char* _hash );

    bool verifySGXSig(const ptr<string>& _sig, const char* _hash);

};

#endif  // OPENSSLECDSAPRIVATEKEY_H
