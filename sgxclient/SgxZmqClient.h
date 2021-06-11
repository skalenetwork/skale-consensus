/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file ZMQClient.h
    @author Stan Kladko
    @date 2021
*/




#ifndef SKALED_SGXZMQCLIENT_H
#define SKALED_SGXZMQCLIENT_H


#define ZMQ_SERVER_ERROR -89
#define ZMQ_COULD_NOT_PARSE -90
#define ZMQ_INVALID_MESSAGE -91
#define ZMQ_COULD_NOT_GET_SOCKOPT -92
#define ZMQ_INVALID_MESSAGE_SIZE -93
#define ZMQ_NO_TYPE_IN_MESSAGE -94
#define ZMQ_NO_SIG_IN_MESSAGE -95
#define ZMQ_NO_CERT_IN_MESSAGE -96
#define ZMQ_COULD_NOT_VERIFY_SIG -97


#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/rand.h>

#include <zmq.hpp>

#include <jsonrpccpp/client.h>

#include "sgxclient/SgxZmqMessage.h"
#include "thirdparty/zguide/zhelpers.hpp"

#define REQUEST_TIMEOUT     10000    //  msecs, (> 1000!)

class SgxZmqClient {

public:

    enum zmq_status {UNKNOWN, TRUE, FALSE};
    zmq_status getZMQStatus() const;
    void setZmqStatus(zmq_status _status);

    uint64_t getZmqSocketCount();

private:

    zmq_status zmqStatus = UNKNOWN;

    EVP_PKEY* pkey = 0;
    EVP_PKEY* pubkey = 0;
    X509* x509Cert = 0;

    bool exited = false;


    zmq::context_t ctx;
    bool sign = true;
    string certKeyName = "";
    string certFileName = "";
    string cert = "";
    string key = "";



    string url;

    // generate random identity

    map<uint64_t , shared_ptr <zmq::socket_t>> clientSockets;
    recursive_mutex socketMutex;

    Schain* schain = nullptr;

public:
    Schain* getSchain() const;

private:

    static cache::lru_cache<string, pair < EVP_PKEY * , X509 *>> verifiedCerts;

    shared_ptr < SgxZmqMessage > doRequestReply(Json::Value &_req,
                               bool _throwExceptionOnTimeout = false);

    string doZmqRequestReply(string &_req, bool _throwExceptionOnTimeout = false);

    uint64_t getProcessID();

    static string readFileIntoString(const string& _fileName);



public:
    SgxZmqClient(Schain* _schain, const string &_domain, uint16_t _port, bool _sign, const string&  _certPathName,
              const string& _certKeyName);

    void reconnect() ;

    static pair<EVP_PKEY*, X509*>  readPublicKeyFromCertStr(const string& _cert);

    static string signString(EVP_PKEY* _pkey, const string& _str);

    string blsSignMessageHash(const string &_keyShareName, const string &_messageHash, 
        int _t, int _n, bool _throwExceptionOnTimeout);

    string ecdsaSignMessageHash(int _base, const string &_keyName, const string &_messageHash,
        bool _throwExceptionOnTimeout);

    void exit();

    static void verifySig(EVP_PKEY* _pubkey, const string& _str, const string& _sig);

    void verifyMsgSig( const char* _msg, size_t _size );


};



#endif
