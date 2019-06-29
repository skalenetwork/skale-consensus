/*
    Copyright (C) 2019 SKALE Labs

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

    @file BlockFinalizeResponseHeader.cpp
    @author Stan Kladko
    @date 2019
*/


#include "../SkaleCommon.h"
#include "../Log.h"
#include "../thirdparty/json.hpp"
#include "../exceptions/InvalidArgumentException.h"

#include "AbstractBlockRequestHeader.h"
#include "BlockFinalizeResponseHeader.h"

BlockFinalizeResponseHeader::BlockFinalizeResponseHeader(): Header(Header::BLOCK_FINALIZE__RSP) {}

void BlockFinalizeResponseHeader::setSigShare(const ptr<string> &_sigShare) {

    if (!_sigShare || _sigShare->size() < 10) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Null or misformatted sig share", __CLASS_NAME__));
    }

    sigShare = _sigShare;
}

void BlockFinalizeResponseHeader::addFields(nlohmann::json &jsonRequest) {

    Header::addFields(jsonRequest);

    if (status != CONNECTION_SUCCESS)
        return;

    if (!sigShare) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Null sig share", __CLASS_NAME__));
    }


    if (!sigShare || sigShare->size() < 10) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Misformatted sig share", __CLASS_NAME__));
    }

    jsonRequest["sigShare"] = *sigShare;

}
