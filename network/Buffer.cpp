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

    @file Buffer.cpp
    @author Stan Kladko
    @date 2018
*/

#include "Log.h"
#include "SkaleCommon.h"
#include "exceptions/FatalError.h"
#include "exceptions/InvalidArgumentException.h"
#include "exceptions/SkaleException.h"

#include "Buffer.h"

void Buffer::write( void* data, size_t dataLen ) {
    CHECK_ARGUMENT(data);
    CHECK_STATE( counter + dataLen <= size );
    memcpy( buf.get()->data() + counter, data, dataLen );
    counter += dataLen;
}

void Buffer::read( void* data, size_t dataLen ) {
    CHECK_ARGUMENT( data );
    CHECK_ARGUMENT( counter + dataLen <= buf->size() );
    memcpy( data, buf.get()->data() + counter, dataLen );
    counter += dataLen;
}

uint64_t Buffer::getCounter() const {
    return counter;
}

Buffer::Buffer( size_t _size ) {
    CHECK_ARGUMENT( _size <= MAX_BUFFER_SIZE );
    this->size = _size;
    buf = make_shared< vector< uint8_t > >( size, 0 );
}

size_t Buffer::getSize() const {
    return size;
}

ptr< vector< uint8_t > > Buffer::getBuf() const {
    CHECK_STATE( buf );
    return buf;
}

void Buffer::consume( char c ) {
    char dummy;
    read( &dummy, sizeof( char ) );
    CHECK_STATE(dummy == c );
}
