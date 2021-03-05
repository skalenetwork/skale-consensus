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

    @file IO.cpp
    @author Stan Kladko
    @date 2018
*/

#include "Log.h"
#include "SkaleCommon.h"
#include "exceptions/FatalError.h"

#include "thirdparty/json.hpp"


#include "Buffer.h"
#include "ClientSocket.h"
#include "IO.h"
#include "ServerConnection.h"
#include "abstracttcpserver/ConnectionStatus.h"
#include "chains/Schain.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/IOException.h"
#include "exceptions/NetworkProtocolException.h"
#include "exceptions/ParsingException.h"
#include "exceptions/PingException.h"
#include "headers/BlockProposalRequestHeader.h"
#include "headers/Header.h"
#include "node/Node.h"

using namespace std;

void IO::readBytes(
    const ptr< ServerConnection >& _env, const ptr< vector< uint8_t > >& _buffer, msg_len len ) {
    CHECK_ARGUMENT( _env );
    CHECK_ARGUMENT( _buffer );
    return readBytes( _env->getDescriptor(), _buffer, len );
}

void IO::readBuf( file_descriptor descriptor, const ptr< Buffer >& _buf, msg_len len ) {
    CHECK_ARGUMENT( _buf );
    CHECK_ARGUMENT( len > 0 );
    CHECK_ARGUMENT( _buf->getSize() >= len );
    return readBytes( descriptor, _buf->getBuf(), len );
}


void IO::readBytes(
    file_descriptor _descriptor, const ptr< vector< uint8_t > >& _buffer, msg_len _len ) {
    // fd_set read_set;
    // struct timeval timeout;

    CHECK_ARGUMENT( _buffer )
    CHECK_ARGUMENT( _len > 0 )
    CHECK_ARGUMENT( _buffer->size() >= _len )

    int64_t bytesRead = 0;

    int64_t result;

    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt( int( _descriptor ), SOL_SOCKET, SO_RCVTIMEO, ( const char* ) &tv, sizeof tv );

    uint64_t counter = 1;

    while ( msg_len( bytesRead ) < _len ) {
        if ( sChain->getNode()->isExitRequested() )
            BOOST_THROW_EXCEPTION( ExitRequestedException( __CLASS_NAME__ ) );


        result = recv(
            int( _descriptor ), _buffer->data() + bytesRead, uint64_t( _len ) - bytesRead, 0 );

        if ( sChain->getNode()->isExitRequested() )
            BOOST_THROW_EXCEPTION( ExitRequestedException( __CLASS_NAME__ ) );


        if ( result == 0 ) {
            BOOST_THROW_EXCEPTION(
                NetworkProtocolException( "The peer shut down the socket, bytes to read:" +
                                          to_string( uint64_t( _len ) - bytesRead ),
                                          __CLASS_NAME__ ) );
        }

        if ( result < 0 && errno == EAGAIN ) {
            result = 0;
        }

        if ( result < 0 ) {
            BOOST_THROW_EXCEPTION( NetworkProtocolException(
                "Read returned error:" + string( strerror( errno ) ), __CLASS_NAME__ ) );
        }

        bytesRead += result;

        counter++;

        if ( counter > 100 ) {
            BOOST_THROW_EXCEPTION(
                NetworkProtocolException( "Peer read timeout", __CLASS_NAME__ ) );
        }

    }

    CHECK_STATE( bytesRead == ( int64_t )( uint64_t ) _len );
}


void IO::writeBytes(
    file_descriptor descriptor, const ptr< vector< uint8_t > >& _buffer, msg_len len ) {
    CHECK_ARGUMENT( _buffer );
    CHECK_ARGUMENT( !_buffer->empty() );
    CHECK_ARGUMENT( len <= _buffer->size() )
    CHECK_ARGUMENT( len > 0 );
    CHECK_ARGUMENT( descriptor != 0 );


    if ( sChain->getNode()->getSimulateNetworkWriteDelayMs() > 0 ) {
        usleep( sChain->getNode()->getSimulateNetworkWriteDelayMs() * 1000 );
    }

    uint64_t bytesWritten = 0;

    while ( msg_len( bytesWritten ) < len ) {
        int64_t result = write(
            ( int ) descriptor, _buffer->data() + bytesWritten, ( uint64_t ) len - bytesWritten );
        if ( sChain->getNode()->isExitRequested() )
            BOOST_THROW_EXCEPTION( ExitRequestedException( __CLASS_NAME__ ) );

        CHECK_STATE( result != 0 );

        if ( result < 1 ) {
            BOOST_THROW_EXCEPTION( IOException( "Could not write bytes", errno, __CLASS_NAME__ ) );
        }

        bytesWritten += result;
    }

    CHECK_STATE( bytesWritten == len );
}


void IO::writeBuf( file_descriptor _descriptor, const ptr< Buffer >& _buf ) {
    CHECK_ARGUMENT( _buf );
    CHECK_ARGUMENT( _buf->getBuf() );
    writeBytes( _descriptor, _buf->getBuf(), msg_len( _buf->getCounter() ) );
}

void IO::writeMagic( const ptr< ClientSocket >& _socket, bool _isPing ) {
    CHECK_ARGUMENT( _socket );

    uint64_t magic;

    if ( _isPing ) {
        magic = TEST_MAGIC_NUMBER;
    } else {
        magic = MAGIC_NUMBER;
    }

    auto buf = make_shared< vector< uint8_t > >( sizeof( magic ) );

    memcpy( buf->data(), &magic, sizeof( magic ) );

    try {
        writeBytesVector( _socket->getDescriptor(), buf );
    } catch ( ExitRequestedException& ) {
        throw;
    }

    catch ( ... ) {
        throw;
    }
}


void IO::writeHeader( const ptr< ClientSocket >& _socket, const ptr< Header >& _header ) {
    CHECK_ARGUMENT( _socket );
    CHECK_ARGUMENT( _header );
    CHECK_ARGUMENT( _header->isComplete() );
    writeBuf( _socket->getDescriptor(), _header->toBuffer() );
}

void IO::writeBytesVector( file_descriptor _socket, const ptr< vector< uint8_t > >& _bytes ) {
    writeBytes( _socket, _bytes, msg_len( _bytes->size() ) );
}

void IO::writePartialHashes(
    file_descriptor _socket, const ptr< map< uint64_t, ptr< partial_sha_hash > > >& _hashes ) {
    CHECK_ARGUMENT( _hashes );
    CHECK_ARGUMENT( _hashes->size() > 0 );

    auto buffer = make_shared< vector< uint8_t > >( _hashes->size() * PARTIAL_HASH_LEN );

    uint64_t counter = 0;
    for ( auto&& item : *_hashes ) {
        memcpy(
            buffer->data() + counter * PARTIAL_HASH_LEN, item.second->data(), PARTIAL_HASH_LEN );
        counter++;
    }

    return writeBytesVector( _socket, buffer );
}

IO::IO( Schain* _sChain ) : sChain( _sChain ) {
    CHECK_ARGUMENT( _sChain );
};


void IO::readMagic( file_descriptor descriptor ) {
    uint64_t magic;

    auto readBuffer = make_shared< vector< uint8_t > >( sizeof( magic ) );

    try {
        readBytes( descriptor, readBuffer, sizeof( magic ) );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested(
            NetworkProtocolException( "Could not read magic number", __CLASS_NAME__ ) );
    }

    magic = *( uint64_t* ) readBuffer->data();

    if ( magic != MAGIC_NUMBER ) {
        if ( magic == TEST_MAGIC_NUMBER ) {
            BOOST_THROW_EXCEPTION( PingException( "Got ping", __CLASS_NAME__ ) );
        }
        BOOST_THROW_EXCEPTION( NetworkProtocolException(
            "Incorrect magic number" + to_string( magic ), __CLASS_NAME__ ) );
    }
}

nlohmann::json IO::readJsonHeader(
    file_descriptor descriptor, const char* _errorString, uint64_t _maxHeaderLen ) {
    CHECK_ARGUMENT( _errorString );

    auto buf2 = make_shared< vector< uint8_t > >( sizeof( uint64_t ) );

    try {
        readBytes( descriptor, buf2, msg_len( sizeof( uint64_t ) ) );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( NetworkProtocolException(
            _errorString + string( ":Could not read header len" ), __CLASS_NAME__ ) );
    }


    uint64_t headerLen = *( uint64_t* ) buf2->data();

    if ( headerLen < 2 || headerLen > _maxHeaderLen ) {
        LOG( err, "Total Len:" + to_string( headerLen ) );
        BOOST_THROW_EXCEPTION( ParsingException(
            _errorString + string( ":Invalid Header len" ) + to_string( headerLen ),
            __CLASS_NAME__ ) );
    }

    ptr< Buffer > buf = make_shared< Buffer >( headerLen );

    try {
        sChain->getIo()->readBuf( descriptor, buf, msg_len( headerLen ) );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( NetworkProtocolException(
            _errorString + string( ":Could not read msg_len bytes from buffer:" ) +
                to_string( headerLen ),
            __CLASS_NAME__ ) );
    }

    auto s = make_shared< string >(
        ( const char* ) buf->getBuf()->data(), ( size_t ) buf->getBuf()->size() );

    LOG( trace, "Read JSON header" + *s );

    nlohmann::json js;

    try {
        js = nlohmann::json::parse( *s );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( ParsingException(
            string( _errorString ) + ":Could not parse request" + *s, __CLASS_NAME__ ) );
    }

    return js;
};
