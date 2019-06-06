/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file Buffer.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/Exception.h"
#include "../exceptions/InvalidArgumentException.h"
#include "../exceptions/FatalError.h"

#include "Buffer.h"

void Buffer::write(void *data, size_t dataLen) {

    ASSERT(counter + dataLen <= size);

    memcpy(buf.get()->data() + counter, data, dataLen);

    counter += dataLen;

}

void Buffer::read(void *data, size_t dataLen) {

    if (counter + dataLen > buf->size()) {
        BOOST_THROW_EXCEPTION(FatalError("Overflowing buffer read:" +
        to_string(counter + dataLen) + ":" + to_string(buf->size()), __CLASS_NAME__));
    }
    memcpy(data, buf.get()->data() + counter, dataLen);
    counter += dataLen;
}

uint64_t Buffer::getCounter() const {
    return counter;
}

Buffer::Buffer(size_t _size) {
    if (_size > MAX_BUFFER_SIZE) {
        throw InvalidArgumentException("Buffer size too large", __CLASS_NAME__);
    }
    this->size = _size;
    buf = make_shared<vector<uint8_t>>(size);
    std::fill(buf->begin(), buf->end(), 0);

}

size_t Buffer::getSize() const {
    return size;
}

const ptr<vector<uint8_t>> &Buffer::getBuf() const {
    return buf;
}

void Buffer::consume(char c) {

    char dummy;

    read(&dummy, sizeof(char));

    if(dummy != c) {
        ASSERT(0);
    }

}
