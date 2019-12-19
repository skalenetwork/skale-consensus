/*
    Copyright (C) 2019-Present SKALE Labs

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

    @file CommittedBlockHeader.cpp
    @author Stan Kladko
    @date 2019
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "thirdparty/json.hpp"
#include "CommittedBlockHeader.h"

CommittedBlockHeader::CommittedBlockHeader(BlockProposal &block, const ptr<string> &thresholdSig) : BlockProposalHeader(
        block), thresholdSig(thresholdSig) {
    CHECK_ARGUMENT(thresholdSig != nullptr);
}

CommittedBlockHeader::CommittedBlockHeader(nlohmann::json &json) : BlockProposalHeader(json) {
    thresholdSig = Header::getString(json, "thrSig");
    CHECK_STATE(thresholdSig != nullptr);
}

const ptr<string> &CommittedBlockHeader::getThresholdSig() const {
    return thresholdSig;
}

void CommittedBlockHeader::addFields(nlohmann::basic_json<> &j) {
    BlockProposalHeader::addFields(j);

    j["thrSig"] = *thresholdSig;
}


