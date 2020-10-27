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

#include "CommittedBlockHeader.h"


CommittedBlockHeader::CommittedBlockHeader(BlockProposal &block, const string &thresholdSig) : BlockProposalHeader(
        block), thresholdSig(thresholdSig) {
    CHECK_ARGUMENT(!thresholdSig.empty())
}

CommittedBlockHeader::CommittedBlockHeader(rapidjson::Document &json) : BlockProposalHeader(json) {
    thresholdSig = Header::getStringRapid(json, "thrSig");
    CHECK_STATE(!thresholdSig.empty())
}

const string &CommittedBlockHeader::getThresholdSig() const {
    CHECK_STATE(!thresholdSig.empty())
    return thresholdSig;
}

void CommittedBlockHeader::addFields(rapidjson::Writer<rapidjson::StringBuffer> &j) {
    BlockProposalHeader::addFields(j);

    j.String("thrSig");
    j.String(thresholdSig.c_str());
}


