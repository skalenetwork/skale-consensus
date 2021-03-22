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

    @file Utils.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once

#include <cstdint>
#include "abstracttcpserver/ConnectionStatus.h"

class Utils {

    static const uint64_t TIME_START = 2208988800;

public:

    static ptr<vector<uint8_t>> u256ToBigEndianArray(const u256 &_value);

    static void checkTime();

    static bool isValidIpAddress(const string& ipAddress);

    static string carray2Hex(const uint8_t *d, size_t _len);

    static uint char2int( char _input );

    static void cArrayFromHex(const string &_hex, uint8_t *_data, size_t len);
};

