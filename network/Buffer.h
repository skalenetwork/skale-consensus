#pragma once

class Buffer {

    size_t size;

    uint64_t counter = 0;

    ptr<vector<uint8_t>> buf;

public:

    void write(void *data, size_t dataLen);

    Buffer(size_t size);

    const ptr<vector<uint8_t>> &getBuf() const;

    void read(void *data, size_t dataLen);

    void consume(char c);



    uint64_t getCounter() const;

    size_t getSize() const;
};
