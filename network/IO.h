#pragma once


class Buffer;

class BlockRetrievalRequestHeader;

class Connection;

class Header;

class Buffer;

class ClientSocket;

class Schain;

class IO {

private:

    Schain *sChain;
public:
    IO(Schain *_sChain);

public:

    void readBytes(ptr<Connection> env, in_buffer *buffer, msg_len len);


    void readBytes(file_descriptor descriptor, in_buffer *buffer, msg_len len);

    void readBuf(file_descriptor descriptor, ptr<Buffer> buf, msg_len len);


    void writeBytes(file_descriptor descriptor, out_buffer *buffer, msg_len len);

    void writeBuf(file_descriptor descriptor, ptr<Buffer> buf);


    void writeHeader(ptr<ClientSocket> socket, ptr<Header> header);




    void writeMagic(ptr<ClientSocket> _socket, bool _isPing = false);

    void writeBytesVector(file_descriptor socket, ptr<vector<uint8_t>> bytes);


    void writePartialHashes(file_descriptor socket, ptr<map<uint64_t, ptr<partial_sha_hash>>> hashes);


    void readMagic(file_descriptor descriptor);

    nlohmann::json readJsonHeader(file_descriptor descriptor, const char* _errorString);






};





