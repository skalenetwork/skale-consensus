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

    @file IO.h
    @author Stan Kladko
    @date 2018
*/

#pragma once


class Buffer;

class BlockRetrievalRequestHeader;

class ServerConnection;

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

    void readBytes(ptr<ServerConnection> _env, ptr<vector<uint8_t>> _buffer, msg_len _len);

    void readBytes(file_descriptor _descriptor, ptr<vector<uint8_t>> _buffer, msg_len _len);

    void readBuf(file_descriptor _descriptor, ptr<Buffer> _buf, msg_len _len);

    void writeBytes(file_descriptor descriptor, ptr<vector<uint8_t>> _buffer, msg_len len);

    void writeBuf(file_descriptor _descriptor, ptr<Buffer> _buf);


    void writeHeader(ptr<ClientSocket> _socket, ptr<Header> _header);




    void writeMagic(ptr<ClientSocket> _socket, bool _isPing = false);

    void writeBytesVector(file_descriptor socket, ptr<vector<uint8_t>> bytes);


    void writePartialHashes(file_descriptor socket, ptr<map<uint64_t, ptr<partial_sha_hash>>> hashes);


    void readMagic(file_descriptor descriptor);

    nlohmann::json readJsonHeader(file_descriptor descriptor, const char* _errorString);






};





