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

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "../thirdparty/json.hpp"


#include "../abstracttcpserver/ConnectionStatus.h"
#include "../node/Node.h"
#include "../headers/Header.h"
#include "../headers/BlockProposalHeader.h"
#include "ClientSocket.h"
#include "../exceptions/NetworkProtocolException.h"
#include "../exceptions/IOException.h"
#include "../exceptions/ParsingException.h"
#include "../exceptions/PingException.h"
#include "../exceptions/ExitRequestedException.h"
#include "../chains/Schain.h"
#include "Buffer.h"
#include "ServerConnection.h"
#include "IO.h"

using namespace std;

void IO::readBytes(ptr<ServerConnection> env, in_buffer *buffer, msg_len len) {
    return readBytes(env->getDescriptor(), buffer, len);
}

void IO::readBuf(file_descriptor descriptor, ptr<Buffer> buf, msg_len len) {
    CHECK_ARGUMENT(buf != nullptr);
    CHECK_ARGUMENT(len > 0);
    CHECK_ARGUMENT(buf->getSize() >= len);

    return readBytes(descriptor, reinterpret_cast< in_buffer * >( buf->getBuf()->data()), len);
}

void IO::readBytes(file_descriptor descriptor, in_buffer *buffer, msg_len len) {
    // fd_set read_set;
    // struct timeval timeout;

    CHECK_ARGUMENT(buffer != nullptr);
    CHECK_ARGUMENT(len > 0);

    int64_t bytesRead = 0;

    int64_t result;


    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(int(descriptor), SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof tv);


    while (msg_len(bytesRead) < len) {


        if (sChain->getNode()->isExitRequested())
            BOOST_THROW_EXCEPTION(ExitRequestedException(__CLASS_NAME__));

        uint64_t counter = 1;

        do {

            result = recv(int(descriptor), buffer + bytesRead, uint64_t(len) - bytesRead, 0);


            if (sChain->getNode()->isExitRequested())
                BOOST_THROW_EXCEPTION(ExitRequestedException(__CLASS_NAME__));
            if (result == 0) {
                usleep(10); // dont do busy wait
            };
            counter++;

            if (counter > 1000) {
                BOOST_THROW_EXCEPTION(NetworkProtocolException("Peer read timeout", __CLASS_NAME__));
            }


        } while (result <= 0 && errno == EAGAIN);

        if (result < 0) {
            BOOST_THROW_EXCEPTION(
                    NetworkProtocolException("Read returned error:" + string(strerror(errno)), __CLASS_NAME__));
        }

        if (result == 0) {
            BOOST_THROW_EXCEPTION(NetworkProtocolException("The peer shut down the socket, bytes to read:" +
                                                           to_string(uint64_t(len) - bytesRead), __CLASS_NAME__));

        }

        bytesRead += result;

        // LOG(trace, "IO bytes read:" + to_string( bytesRead ) );
    }

    assert ((uint64_t ) bytesRead == (uint64_t ) len);

}

void IO::writeBytes(file_descriptor descriptor, out_buffer *buffer, msg_len len) {

    usleep(sChain->getNode()->getSimulateNetworkWriteDelayMs() * 1000);

    CHECK_ARGUMENT(buffer);
    CHECK_ARGUMENT(len > 0);
    CHECK_ARGUMENT(descriptor != 0);

    //    setsockopt( int( descriptor ), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof( int ) );




    uint64_t bytesWritten = 0;

    while (msg_len(bytesWritten) < len) {
        int64_t result =
                write((int) descriptor, buffer + bytesWritten, (uint64_t) len - bytesWritten);


        if (sChain->getNode()->isExitRequested())
            BOOST_THROW_EXCEPTION(ExitRequestedException(__CLASS_NAME__));

        if (result < 1) {
            BOOST_THROW_EXCEPTION(IOException("Could not write bytes", errno, __CLASS_NAME__));
        }


        bytesWritten += result;
    }
}

void IO::writeBuf(file_descriptor descriptor, ptr<Buffer> buf) {
    CHECK_ARGUMENT(buf != nullptr);
    CHECK_ARGUMENT(buf->getBuf() != nullptr);
    writeBytes(descriptor, (out_buffer *) buf->getBuf()->data(), msg_len(buf->getCounter()));
}

void IO::writeMagic(ptr<ClientSocket> _socket, bool _isPing) {


    uint64_t magic;

    if (_isPing) {
        magic = TEST_MAGIC_NUMBER;
    } else {
        magic = MAGIC_NUMBER;
    }

    try {
        writeBytes(_socket->getDescriptor(), (out_buffer *) &magic, sizeof(uint64_t));
    } catch (ExitRequestedException &) { throw; }

    catch (...) {
        throw;
    }

}


void IO::writeHeader(ptr<ClientSocket> socket, ptr<Header> header) {
    CHECK_ARGUMENT(socket);
    CHECK_ARGUMENT(header);
    CHECK_ARGUMENT(header->isComplete());
    writeBuf(socket->getDescriptor(), header->toBuffer());
}

void IO::writeBytesVector(file_descriptor socket, ptr<vector<uint8_t> > bytes) {
    CHECK_ARGUMENT(bytes != nullptr);
    CHECK_ARGUMENT(!bytes->empty());
    writeBytes(socket, (out_buffer *) bytes->data(), msg_len(bytes->size()));
}

void IO::writePartialHashes(
        file_descriptor socket, ptr<map<uint64_t, ptr<partial_sha_hash>>> hashes) {
    CHECK_ARGUMENT(hashes->size() > 0);

    auto buffer = make_shared<vector<uint8_t> >(hashes->size() * PARTIAL_SHA_HASH_LEN);

    uint64_t counter = 0;
    for (auto &&item: *hashes) {
        memcpy(buffer->data() + counter * PARTIAL_SHA_HASH_LEN, item.second->data(),
               PARTIAL_SHA_HASH_LEN);
        counter++;
    }

    return writeBytesVector(socket, buffer);
}

IO::IO(Schain *_sChain) : sChain(_sChain) {
    CHECK_ARGUMENT(_sChain);
};


void IO::readMagic(file_descriptor descriptor) {
    uint64_t magic;

    try {
        readBytes(descriptor, (in_buffer *) &magic, sizeof(magic));
    } catch (ExitRequestedException &) { throw; }
    catch (...) {
        throw_with_nested(NetworkProtocolException("Could not read magic number", __CLASS_NAME__));
    }

    if (magic != MAGIC_NUMBER) {
        if (magic == TEST_MAGIC_NUMBER) {
            BOOST_THROW_EXCEPTION(PingException("Got ping", __CLASS_NAME__));
        }
        BOOST_THROW_EXCEPTION(NetworkProtocolException("Incorrect magic number" + to_string(magic), __CLASS_NAME__));
    }


}

nlohmann::json IO::readJsonHeader(file_descriptor descriptor, const char *_errorString) {


    auto buf2 = make_shared<array<uint64_t, MAX_HEADER_SIZE>>();


    ptr<Buffer> buf = nullptr;

    try {
        readBytes(descriptor,
                  (in_buffer *) buf2->data(),
                  msg_len(sizeof(uint64_t)));
    } catch (ExitRequestedException &) { throw; }
    catch (...) {
        throw_with_nested(NetworkProtocolException(_errorString + string(":Could not read header"), __CLASS_NAME__));
    }


    uint64_t headerLen = (*buf2).at(0);

    if (headerLen < 2 || headerLen >= MAX_HEADER_SIZE) {
        LOG(err, "Total Len:" + to_string(headerLen));
        BOOST_THROW_EXCEPTION(
                ParsingException(_errorString + string(":Invalid Header len") + to_string(headerLen), __CLASS_NAME__));
    }

    buf = make_shared<Buffer>(headerLen);

    try {
        sChain->getIo()->
                readBuf(descriptor, buf,
                        msg_len(headerLen));
    } catch (ExitRequestedException &) { throw; }
    catch (...) {
        throw_with_nested(
                NetworkProtocolException(
                        _errorString + string(":Could not read msg_len bytes from buffer:") + to_string(headerLen),
                        __CLASS_NAME__));
    }


    auto s = make_shared<string>((const char *) buf->getBuf()->data(), (size_t) buf->getBuf()->size());


    LOG(trace, "Read JSON header" + *s);

    nlohmann::json js;

    try {
        js = nlohmann::json::parse(*s);
    } catch (ExitRequestedException &) { throw; }
    catch (...) {
        BOOST_THROW_EXCEPTION(ParsingException(string(_errorString) + ":Could not parse request" + *s, __CLASS_NAME__));
    }

    return js;


};
