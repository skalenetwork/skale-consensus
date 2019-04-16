//
// Created by stan on 31.07.18.
//

#include "../SkaleConfig.h"
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
    ASSERT(counter + dataLen <= buf->size());
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
