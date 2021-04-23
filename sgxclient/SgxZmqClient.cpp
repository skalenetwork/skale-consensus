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

    @file ZMQClient.cpp
    @author Stan Kladko
    @date 2020
*/

#include "sys/random.h"
#include <sys/syscall.h>
#include <sys/types.h>


#include <fstream>
#include <regex>
#include <streambuf>


#include "BLSSignReqMessage.h"
#include "BLSSignRspMessage.h"
#include "ECDSASignReqMessage.h"
#include "ECDSASignRspMessage.h"
#include "Log.h"
#include "SgxZmqClient.h"
#include "SkaleCommon.h"
#include "chains/Schain.h"
#include "network/Utils.h"


shared_ptr< SgxZmqMessage > SgxZmqClient::doRequestReply( Json::Value& _req ) {
    Json::FastWriter fastWriter;
    fastWriter.omitEndingLineFeed();

    static uint64_t i = 0;

    if ( sign ) {
        CHECK_STATE( !cert.empty() );
        CHECK_STATE( !key.empty() );
        _req["cert"] = cert;
    }

    string reqStr = fastWriter.write( _req );

    CHECK_STATE( reqStr.back() == '}' );

    if ( sign ) {
        auto sig = signString( pkey, reqStr );
        reqStr.back() = ',';
        reqStr.append( "\"msgSig\":\"" );
        reqStr.append( sig );
        reqStr.append( "\"}" );
    }



    if ( i % 10 == 0 ) {  // verify each 10th sig
        verifyMsgSig( reqStr.c_str(), reqStr.length() );
    }

    i++;

    CHECK_STATE( reqStr.front() == '{' );
    CHECK_STATE( reqStr.back() == '}' );

    auto resultStr = doZmqRequestReply( reqStr );


    try {
        CHECK_STATE( resultStr.size() > 5 )
        CHECK_STATE( resultStr.front() == '{' )
        CHECK_STATE( resultStr.back() == '}' )


        return SgxZmqMessage::parse( resultStr.c_str(), resultStr.size(), false );
    } catch ( std::exception& e ) {
        spdlog::error( string( "Error in doRequestReply:" ) + e.what() );
        throw;
    } catch ( ... ) {
        spdlog::error( "Error in doRequestReply" );
        throw;
    }
}


string SgxZmqClient::doZmqRequestReply( string& _req ) {
    stringstream request;

    shared_ptr< zmq::socket_t > clientSocket = nullptr;

    {
        lock_guard< recursive_mutex > m( mutex );
        if ( !clientSockets.count( getProcessID() ) )
            reconnect();
        clientSocket = clientSockets.at( getProcessID() );
        CHECK_STATE( clientSocket );
    }
    CHECK_STATE( clientSocket );

    spdlog::debug( "ZMQ client sending: \n {}", _req );

    s_send( *clientSocket, _req );

    while ( true ) {
        //  Poll socket for a reply, with timeout
        zmq::pollitem_t items[] = { { static_cast< void* >( *clientSocket ), 0, ZMQ_POLLIN, 0 } };
        zmq::poll( &items[0], 1, REQUEST_TIMEOUT );

        schain->getNode()->exitCheck();

        //  If we got a reply, process it
        if ( items[0].revents & ZMQ_POLLIN ) {
            string reply = s_recv( *clientSocket );

            CHECK_STATE( reply.size() > 5 );
            reply = reply.substr( 0, reply.size() - 1 );
            LOG( debug, "ZMQ client received reply:" + reply );
            CHECK_STATE( reply.front() == '{' );
            CHECK_STATE( reply.back() == '}' );

            return reply;
        } else {
            LOG( err, "W: no response from server, retrying..." );
            reconnect();
            //  Send request again, on new socket
            s_send( *clientSocket, _req );
        }
    }
}

string SgxZmqClient::readFileIntoString( const string& _fileName ) {
    ifstream t( _fileName );
    t.exceptions( t.failbit | t.badbit | t.eofbit );

    string str;

    try {
        str = string( ( istreambuf_iterator< char >( t ) ), istreambuf_iterator< char >() );
    } catch ( ... ) {
        LOG( err, "Could not read file:" + _fileName );
        throw;
    }

    return str;
}


string SgxZmqClient::signString( EVP_PKEY* _pkey, const string& _str ) {
    CHECK_STATE( _pkey );
    CHECK_STATE( !_str.empty() );

    static std::regex r( "\\s+" );
    auto msgToSign = std::regex_replace( _str, r, "" );


    EVP_MD_CTX* mdctx = NULL;
    unsigned char* signature = NULL;
    size_t slen = 0;

    CHECK_STATE( mdctx = EVP_MD_CTX_create() );

    CHECK_STATE( ( EVP_DigestSignInit( mdctx, NULL, EVP_sha256(), NULL, _pkey ) == 1 ) );


    CHECK_STATE( EVP_DigestSignUpdate( mdctx, msgToSign.c_str(), msgToSign.size() ) == 1 );

    /* First call EVP_DigestSignFinal with a NULL sig parameter to obtain the length of the
     * signature. Length is returned in slen */

    CHECK_STATE( EVP_DigestSignFinal( mdctx, NULL, &slen ) == 1 );
    signature = ( unsigned char* ) OPENSSL_malloc( sizeof( unsigned char ) * slen );
    CHECK_STATE( signature );
    CHECK_STATE( EVP_DigestSignFinal( mdctx, signature, &slen ) == 1 );

    auto hexSig = Utils::carray2Hex( signature, slen );

    string hexStringSig( hexSig.begin(), hexSig.end() );

    /* Clean up */
    if ( signature )
        OPENSSL_free( signature );
    if ( mdctx )
        EVP_MD_CTX_destroy( mdctx );

    return hexStringSig;
}

pair< EVP_PKEY*, X509* > SgxZmqClient::readPublicKeyFromCertStr( const string& _certStr ) {
    CHECK_STATE( !_certStr.empty() )

    BIO* bo = BIO_new( BIO_s_mem() );
    CHECK_STATE( bo );
    CHECK_STATE( BIO_write( bo, _certStr.c_str(), _certStr.size() > 0 ) );

    X509* cert = nullptr;
    CHECK_STATE( PEM_read_bio_X509( bo, &cert, 0, 0 ) );
    CHECK_STATE( cert );
    auto key = X509_get_pubkey( cert );
    BIO_free( bo );
    CHECK_STATE( key );
    return { key, cert };
};

SgxZmqClient::SgxZmqClient( Schain* _sChain, const string& ip, uint16_t port, bool _sign,
    const string& _certFileName, const string& _certKeyName )
    : ctx( 1 ), sign( _sign ), certKeyName( _certKeyName ), certFileName( _certFileName ) {
    CHECK_STATE( _sChain );
    this->schain = _sChain;

    LOG( info, "Initing ZMQClient. Sign:" + to_string( _sign ) );

    if ( sign ) {
        CHECK_STATE( !_certFileName.empty() );
        try {
            cert = readFileIntoString( _certFileName );
        } catch ( exception& e ) {
            LOG( err, "Could not read file:" + _certFileName
                + ":" + e.what());
            throw;
        }
        CHECK_STATE( !cert.empty() );

        try {
            key = readFileIntoString( _certKeyName );
        } catch ( exception& e ) {
            LOG( err, "Could not read file:" + _certKeyName
                + ":" + e.what());
            throw;
        }

        CHECK_STATE( !key.empty() );

        BIO* bo = BIO_new( BIO_s_mem() );
        CHECK_STATE( bo );
        CHECK_STATE( bo );
        BIO_write( bo, key.c_str(), key.size() );

        PEM_read_bio_PrivateKey( bo, &pkey, 0, 0 );
        BIO_free( bo );
        CHECK_STATE( pkey );

        tie( pubkey, x509Cert ) = readPublicKeyFromCertStr( cert );

        auto sig = signString( pkey, "sample" );

    } else {
        CHECK_STATE( _certFileName.empty() );
        CHECK_STATE( _certKeyName.empty() );
    }

    certFileName = _certFileName;
    certKeyName = _certKeyName;

    url = "tcp://" + ip + ":" + to_string( port );
}

void SgxZmqClient::reconnect() {
    lock_guard< recursive_mutex > lock( mutex );

    auto pid = getProcessID();

    if ( clientSockets.count( pid ) > 0 ) {
        clientSockets.erase( pid );
    }

    uint64_t  randNumber;
    CHECK_STATE(getrandom( &randNumber, sizeof(uint64_t), 0 ) == sizeof(uint64_t));

    string identity = to_string((uint64_t) getSchain()->getSchainIndex()) +
        ":" + to_string(randNumber);

    auto clientSocket = make_shared< zmq::socket_t >( ctx, ZMQ_DEALER );
    clientSocket->setsockopt( ZMQ_IDENTITY, identity.c_str(),  identity.size() + 1);
    //  Configure socket to not wait at close time
    int linger = 0;
    clientSocket->setsockopt( ZMQ_LINGER, &linger, sizeof( linger ) );
    clientSocket->connect( url );
    clientSockets.insert( { pid, clientSocket } );
}


string SgxZmqClient::blsSignMessageHash(
    const std::string& keyShareName, const std::string& messageHash, int t, int n ) {
    Json::Value p;
    p["type"] = SgxZmqMessage::BLS_SIGN_REQ;
    p["keyShareName"] = keyShareName;
    p["messageHash"] = messageHash;
    p["n"] = n;
    p["t"] = t;
    auto result = dynamic_pointer_cast< BLSSignRspMessage >( doRequestReply( p ) );
    CHECK_STATE( result );
    CHECK_STATE( result->getStatus() == 0 );

    return result->getSigShare();
}

string SgxZmqClient::ecdsaSignMessageHash(
    int base, const std::string& keyName, const std::string& messageHash ) {
    Json::Value p;
    p["type"] = SgxZmqMessage::ECDSA_SIGN_REQ;
    p["base"] = base;
    p["keyName"] = keyName;
    p["messageHash"] = messageHash;
    auto result = dynamic_pointer_cast< ECDSASignRspMessage >( doRequestReply( p ) );
    CHECK_STATE( result );
    CHECK_STATE( result->getStatus() == 0 );
    return result->getSignature();
}


uint64_t SgxZmqClient::getProcessID() {
    return syscall( __NR_gettid );
}
void SgxZmqClient::exit() {
    this->ctx.shutdown();
    this->clientSockets.clear();
}


void SgxZmqClient::verifyMsgSig( const char* _msg, size_t ) {
    CHECK_STATE( _msg );

    auto d = make_shared< rapidjson::Document >();

    d->Parse( _msg );

    CHECK_STATE( !d->HasParseError() );
    CHECK_STATE( d->IsObject() );

    CHECK_STATE( d->HasMember( "type" ) );
    CHECK_STATE( ( *d )["type"].IsString() );
    string type = ( *d )["type"].GetString();

    CHECK_STATE( d->HasMember( "cert" ) );
    CHECK_STATE( d->HasMember( "msgSig" ) );
    CHECK_STATE( ( *d )["cert"].IsString() );
    CHECK_STATE( ( *d )["msgSig"].IsString() );

    auto cert = make_shared< string >( ( *d )["cert"].GetString() );

    static recursive_mutex m;

    EVP_PKEY* publicKey = nullptr;

    {
        lock_guard< recursive_mutex > lock( m );

        if ( !verifiedCerts.exists( *cert ) ) {
            auto handles = SgxZmqClient::readPublicKeyFromCertStr( *cert );
            CHECK_STATE( handles.first );
            CHECK_STATE( handles.second );
            verifiedCerts.put( *cert, handles );
            remove( cert->c_str() );
        }

        publicKey = verifiedCerts.get( *cert ).first;


        CHECK_STATE( publicKey );

        auto msgSig = make_shared< string >( ( *d )["msgSig"].GetString() );

        d->RemoveMember( "msgSig" );

        rapidjson::StringBuffer buffer;

        rapidjson::Writer< rapidjson::StringBuffer > w( buffer );

        d->Accept( w );

        string msgToVerify = buffer.GetString();

        SgxZmqClient::verifySig( publicKey, msgToVerify, *msgSig );
    }
}

void SgxZmqClient::verifySig( EVP_PKEY* _pubkey, const string& _str, const string& _sig ) {
    CHECK_STATE( _pubkey );
    CHECK_STATE( !_str.empty() );

    static std::regex r( "\\s+" );
    auto msgToSign = std::regex_replace( _str, r, "" );

    vector< uint8_t > binSig( 256, 0 );


    uint64_t binLen = _sig.length() / 2;

    Utils::cArrayFromHex( _sig, binSig.data(), binLen );

    EVP_MD_CTX* mdctx = NULL;

    CHECK_STATE( mdctx = EVP_MD_CTX_create() );

    CHECK_STATE( ( EVP_DigestVerifyInit( mdctx, NULL, EVP_sha256(), NULL, _pubkey ) == 1 ) );

    CHECK_STATE( EVP_DigestVerifyUpdate( mdctx, msgToSign.c_str(), msgToSign.size() ) == 1 );

    /* First call EVP_DigestSignFinal with a NULL sig parameter to obtain the length of the
     * signature. Length is returned in slen */


    CHECK_STATE2( EVP_DigestVerifyFinal( mdctx, binSig.data(), binLen ) == 1,
        "Could not verify msg signature" );

    if ( mdctx )
        EVP_MD_CTX_destroy( mdctx );
}

cache::lru_cache< string, pair< EVP_PKEY*, X509* > > SgxZmqClient::verifiedCerts( 256 );
Schain* SgxZmqClient::getSchain() const {
    CHECK_STATE(schain);
    return schain;
}
