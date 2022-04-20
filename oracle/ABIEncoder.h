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

class OracleRequestSpec;

#ifndef SKALED_ABIENCODER_H
#define SKALED_ABIENCODER_H


class ABIEncoder {

public:
    static  ptr<vector<uint8_t>> abiEncodeResult(ptr<OracleRequestSpec>, uint64_t,
                                                            ptr<vector<ptr<string>>>);

    static ptr<vector<uint8_t>> abiEncodeUint64(uint64_t _value);


    static ptr<vector<uint8_t>> abiEncodeString(string& _value);


};


#endif //SKALED_ABIENCODER_H
