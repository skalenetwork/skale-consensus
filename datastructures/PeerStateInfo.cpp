/*
    Copyright (C) 2023- SKALE Labs

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

*/

#include "thirdparty/json.hpp"
#include "SkaleCommon.h"
#include "PeerStateInfo.h"

const block_id &PeerStateInfo::getLastBlockId() const {
    return lastBlockId;
}

uint64_t PeerStateInfo::getLastBlockTimestampS() const {
    return lastBlockTimestampS;
}

PeerStateInfo::PeerStateInfo(const block_id &lastBlockId, uint64_t lastBlockTimestampS) : lastBlockId(lastBlockId),
                                                                                          lastBlockTimestampS(
                                                                                                  lastBlockTimestampS) {

}

// extract PeerStateInfo object from catchup response header
ptr<PeerStateInfo> PeerStateInfo::extract(nlohmann::json _catchupResponseHeasder) {

    uint64_t lastBid = 0;
    uint64_t lastTs = 0;

    if (_catchupResponseHeasder.find("lastBid") != _catchupResponseHeasder.end()) {
        lastBid = _catchupResponseHeasder.at("lastBid").get<uint64_t>();
    }

    if (_catchupResponseHeasder.find("lastTs") != _catchupResponseHeasder.end()) {
        lastTs = _catchupResponseHeasder.at("lastTs").get<uint64_t >();
    }

    if (lastBid > 0 && lastTs > 0) {
        return make_shared<PeerStateInfo>(block_id(lastBid), lastTs);
    } else {
        // the node did not provide info
        return nullptr;
    }
}
