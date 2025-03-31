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


#pragma once



class PeerStateInfo {

    block_id lastBlockId = 0;
    uint64_t lastBlockTimestampS = 0;

public:
    const block_id &getLastBlockId() const;

    uint64_t getLastBlockTimestampS() const;

    PeerStateInfo(const block_id &lastBlockId, uint64_t lastBlockTimestampS);

    static ptr<PeerStateInfo> extract(nlohmann::json _catchupResponseHeasder);

};


