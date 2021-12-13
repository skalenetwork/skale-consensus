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

    @file OracleResponse.h
    @author Stan Kladko
    @date 2021 -
*/

#include "SkaleCommon.h"
#include "Log.h"

#include "OracleResponse.h"

OracleResponse::OracleResponse(uint32_t _oracleStatus, const string &_oracleResult) : oracleStatus(_oracleStatus),
                                                                                         oracleResult(_oracleResult) {
    if (oracleStatus == ORACLE_SUCCESS) {
        CHECK_STATE(oracleResult.front() == '{' && oracleResult.back() == '}')
    } else {
        CHECK_STATE(oracleResult.empty())
    }
}
