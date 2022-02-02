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


CommittedBlockHeader::CommittedBlockHeader(BlockProposal &block, const string &thresholdSig,
                    ptr<map<uint64_t, string>> _decryptedTEKeys) : BlockProposalHeader(
        block), thresholdSig(thresholdSig), decryptedArgKeys(_decryptedTEKeys) {
    CHECK_ARGUMENT(!thresholdSig.empty())

    if (!decryptedArgKeys) {
        decryptedArgKeys = make_shared<map<uint64_t, string>>();
    }
}

CommittedBlockHeader::CommittedBlockHeader(nlohmann::json &json) : BlockProposalHeader(json) {
    thresholdSig = Header::getString(json, "thrSig");
    CHECK_STATE(!thresholdSig.empty())

    if (json.find("teks" ) != json.end()) {
        decryptedArgKeys = Header::getIntegerStringMap(json, "teks");
    } else {
        decryptedArgKeys = make_shared<map<uint64_t,string>>();
    }
}

const string &CommittedBlockHeader::getThresholdSig() const {
    CHECK_STATE(!thresholdSig.empty())
    return thresholdSig;
}

void CommittedBlockHeader::addFields(nlohmann::basic_json<> &j) {
    BlockProposalHeader::addFields(j);

    j["thrSig"] = thresholdSig;
}

const ptr<map<uint64_t, string>> &CommittedBlockHeader::getDecryptedArgKeys() const {
    return decryptedArgKeys;
}


