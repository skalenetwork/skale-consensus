/*
    Copyright (C) 2021- SKALE Labs

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

    @file ABIEncoder.h
    @author Stan Kladko
    @date 2021-
*/

#include <algorithm>

extern "C" {
#include "thirdparty/ethereum_abi_c/abi.h"
}
#include "SkaleCommon.h"
#include "Log.h"
#include "network/Utils.h"
#include "OracleRequestSpec.h"
#include "ABIEncoder.h"


#define ARRAY_SIZE(a) sizeof(a)/sizeof(a[0])




ptr<vector<uint8_t>> ABIEncoder::abiEncodeUint64(uint64_t _value) {
    auto result = make_shared<vector<uint8_t>>(sizeof(uint64_t), 0);
    memcpy(result->data(), &_value, sizeof(uint64_t));
    reverse(result->begin(), result->end());
    return result;
}

ptr<vector<uint8_t>> ABIEncoder::abiEncodeString(string& _value) {
    cerr << _value << endl;
    auto result = make_shared<vector<uint8_t>>(_value.size(), 1);
    memcpy(result->data(), _value.c_str(), _value.size() + 1);
    return result;
}



ABI_t result_abi[4] = {
        { .type = ABI_UINT64, .isArray = false, .arraySz = 0},
        { .type = ABI_STRING, .isArray = false, .arraySz = 0},
        { .type = ABI_UINT64, .isArray = false, .arraySz = 0},
        { .type = ABI_UINT64, .isArray = false, .arraySz = 0}
};


ptr<vector<uint8_t>> ABIEncoder::abiEncodeResult(ptr<OracleRequestSpec> _spec, uint64_t _status,
                                                        ptr<vector<ptr<string>>>) {

    vector<uint8_t> fullEncoding;
    vector<size_t> offsets;

    uint8_t outBuf[32 * 1024] = {0};


    auto chainId = _spec->getChainid();
    auto chainIdEncoding = ABIEncoder::abiEncodeUint64(chainId);
    offsets.push_back(fullEncoding.size());
    fullEncoding.insert(fullEncoding.begin(), chainIdEncoding->begin(), chainIdEncoding->end());

    auto uri = _spec->getUri();

    auto uriEncoding = ABIEncoder::abiEncodeString(uri);
    offsets.push_back(fullEncoding.size());
    fullEncoding.insert(fullEncoding.begin(), uriEncoding->begin(), uriEncoding->end());


    auto time = _spec->getTime();
    auto timeEncoding = ABIEncoder::abiEncodeUint64(time);
    offsets.push_back(fullEncoding.size());
    fullEncoding.insert(fullEncoding.begin(), timeEncoding->begin(), timeEncoding->end());

    auto statusEncoding = ABIEncoder::abiEncodeUint64(_status);
    offsets.push_back(fullEncoding.size());
    fullEncoding.insert(fullEncoding.begin(), statusEncoding->begin(), statusEncoding->end());


    uint64_t numBytes = abi_encode(outBuf, ARRAY_SIZE(outBuf), result_abi, ARRAY_SIZE(result_abi),
               offsets.data(), fullEncoding.data(), fullEncoding.size());

    CHECK_STATE(numBytes > 0);

    cerr << numBytes << endl;


    auto hexString = Utils::carray2Hex(outBuf, numBytes);

    cerr << hexString << endl;


    auto result = make_shared<vector<uint8_t>>(numBytes, 0);

    memcpy(result->data(), outBuf, numBytes);

    exit(5);

    return result;

}


void ABIEncoder::healthCheck() {
    Utils::execCommand("ls /usr/bin/ethabi");

    // sudo cargo install --root /usr/ ethabi-cli
    // sudo apt install rustc

}
