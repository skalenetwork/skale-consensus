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

    @file Utils.cpp
    @author Stan Kladko
    @date 2018
*/

#include "thirdparty/json.hpp"

#include "SkaleCommon.h"
#include "Log.h"

#include "exceptions/FatalError.h"
#include "exceptions/InvalidArgumentException.h"

#include "Utils.h"

ptr<vector<uint8_t>> Utils::u256ToBigEndianArray(const u256 &_value) {
// export into 8-bit unsigned values, most significant bit first:
    auto v = make_shared<vector<uint8_t>>();
    export_bits(_value, std::back_inserter(*v), 8);
    return v;
}

void Utils::checkTime() {

    if (getenv("NO_NTP_CHECK") != nullptr) {
        return;
    }

    auto ip = gethostbyname("pool.ntp.org");
    auto fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);


    if (fd < 0) {
        cerr << "Could not open NTP socket" << endl;
        BOOST_THROW_EXCEPTION(FatalError("Can not open NTP socket"));
    }

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error");
    }


    if (!ip) {
        cerr << "Couldnt DNS resolve pool.ntp.org. Check internet connection." << endl;
        BOOST_THROW_EXCEPTION(FatalError("Could not get IP address", __CLASS_NAME__));
    }

    union {
        struct sockaddr_in sa_in;
        struct sockaddr sa;
    } srvAddr;
    memset( &srvAddr, 0, sizeof(srvAddr) );
    memcpy( (void *) &srvAddr.sa_in.sin_addr.s_addr, (void *) ip->h_addr, size_t( ip->h_length ) );
    srvAddr.sa_in.sin_family = AF_INET;
    srvAddr.sa_in.sin_port = htons( 123 );
    if (connect(fd, (struct sockaddr *) &srvAddr.sa_in, sizeof(srvAddr.sa_in)) < 0)
        BOOST_THROW_EXCEPTION(FatalError("Could not connect to NTP server"));

    struct {
        uint8_t vnm = 0x1b;
        uint8_t str = 0;
        uint8_t poll = 0;
        uint8_t prec = 0;
        uint32_t rootDelay = 0;
        uint32_t rootDispersion = 0;
        uint32_t refId = 0;
        uint32_t refTmS = 0;
        uint32_t refTmF = 0;
        uint32_t origTmS = 0;
        uint32_t origTmF = 0;
        uint32_t rxTmS = 0;
        uint32_t rxTmF = 0;
        uint32_t txTmS = 0;
        uint32_t txTmF = 0;

    } ntpMessage;


    if (write(fd, (char *) &ntpMessage, sizeof(ntpMessage)) <= 0)
        BOOST_THROW_EXCEPTION(FatalError("Could not write to NTP server ", __CLASS_NAME__));


    if (read(fd, (char *) &ntpMessage, sizeof(ntpMessage)) != sizeof(ntpMessage)) {
        if (errno != EAGAIN)
            BOOST_THROW_EXCEPTION(FatalError("Could not read from NTP server", __CLASS_NAME__));
        else
            return;
    }

    if (ntpMessage.str < 1 || ntpMessage.str > 15) {
        return;
    }

    int64_t timeDiff = ntohl(ntpMessage.txTmS) - TIME_START - time(NULL);

    if (timeDiff > 1 || timeDiff < -1)
        BOOST_THROW_EXCEPTION(FatalError(
                                      "System time is not synchronized with NTP. \n"
                                      "Please enable NTP by running the following command:"
                                      "\n sudo apt-get install ntp && sudo timedatectl set-ntp on \n"
                                      "Time difference:" + to_string(timeDiff) + ":local:" + to_string(time(NULL)) +
                                      ":ntp.org:" + to_string(ntohl(ntpMessage.txTmS) - TIME_START)));
}


bool Utils::isValidIpAddress(const string& ipAddress) {
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr));
    return result != 0;
}


string Utils::carray2Hex(const uint8_t *d, size_t _len) {
    char hex[2 * _len];

    static char hexval[16] = {
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    for (size_t j = 0; j < _len; j++) {
        hex[j * 2] = hexval[((d[j] >> 4) & 0xF)];
        hex[(j * 2) + 1] = hexval[(d[j]) & 0x0F];
    }

    string result((char *) hex, 2 * _len);
    return result;
}


uint Utils::char2int(char _input) {
    if (_input >= '0' && _input <= '9')
        return _input - '0';
    if (_input >= 'A' && _input <= 'F')
        return _input - 'A' + 10;
    if (_input >= 'a' && _input <= 'f')
        return _input - 'a' + 10;
    BOOST_THROW_EXCEPTION(InvalidArgumentException("Invalid input string", __CLASS_NAME__));
}

void Utils::cArrayFromHex(const string &_hex, uint8_t *_data, size_t len) {

    CHECK_ARGUMENT(_hex.size() % 2 == 0);
    CHECK_ARGUMENT(_hex.size() / 2 == len);

    for (size_t i = 0; i < _hex.size() / 2; i++) {
        _data[i] = Utils::char2int(_hex.at(2 * i)) * 16 + Utils::char2int(_hex.at(2 * i + 1));
    }
}


string execCommand(const string& _cmd) {
    int _exitStatus = 0;

    auto pPipe = ::popen(_cmd.c_str(), "r");

    CHECK_STATE2(pPipe, "Could not open pipe in exec");

    array<char, 256> buffer;

    string result;

    while(not std::feof(pPipe))
    {
        auto bytes = std::fread(buffer.data(), 1, buffer.size(), pPipe);
        result.append(buffer.data(), bytes);
    }

    auto rc = ::pclose(pPipe);

    if(WIFEXITED(rc))
    {
        _exitStatus = WEXITSTATUS(rc);
    }

    CHECK_STATE2(_exitStatus == 0, "Command failure:" + _cmd + ":" + to_string(_exitStatus));

    return result;
}