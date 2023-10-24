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

    @file CatchupResponseHeader.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"

#include "thirdparty/json.hpp"

#include "abstracttcpserver/ConnectionStatus.h"
#include "MissingTransactionsRequestHeader.h"
#include "CatchupResponseHeader.h"


using namespace std;

CatchupResponseHeader::CatchupResponseHeader() : Header( Header::BLOCK_CATCHUP_RSP ) {}

void CatchupResponseHeader::setBlockSizes( const ptr< list< uint64_t > >& _blockSizes ) {
    CHECK_ARGUMENT( _blockSizes );

    blockCount = _blockSizes->size();

    blockSizes = _blockSizes;

    complete = true;
}

void CatchupResponseHeader::addFields( nlohmann::json& _j ) {
    Header::addFields( _j );

    _j["count"] = blockCount;

    if ( blockSizes != nullptr )
        _j["sizes"] = *blockSizes;
}

uint64_t CatchupResponseHeader::getBlockCount() const {
    return blockCount;
}

void CatchupResponseHeader::setBlockCount( uint64_t _blockCount ) {
    CatchupResponseHeader::blockCount = _blockCount;
}
