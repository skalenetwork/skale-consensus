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

#include "thirdparty/json.hpp"

#include "SkaleCommon.h"
#include "Log.h"

#include "datastructures/CommittedBlock.h"
#include "CommittedBlockHeader.h"



CommittedBlockHeader::CommittedBlockHeader(CommittedBlock &_block) : BlockProposalHeader(
        _block), thresholdSig(_block.getThresholdSig()), daSig(_block.getDaSig()) {
    CHECK_ARGUMENT(!thresholdSig.empty())
}

CommittedBlockHeader::CommittedBlockHeader(nlohmann::json &_json) : BlockProposalHeader(_json) {
    thresholdSig = Header::getString(_json, "thrSig");
    CHECK_STATE(!thresholdSig.empty())
    daSig = Header::maybeGetString(_json, "daSig");
}

const string &CommittedBlockHeader::getThresholdSig() const {
    CHECK_STATE(!thresholdSig.empty())
    return thresholdSig;
}

const string &CommittedBlockHeader::getDaSig() const {
    return daSig;
}



void CommittedBlockHeader::addFields(nlohmann::basic_json<> &_j) {
    BlockProposalHeader::addFields(_j);

    _j["thrSig"] = thresholdSig;

    if (!daSig.empty()) {
        _j["daSig"] = daSig;
    }

}


