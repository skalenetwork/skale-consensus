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

    @file CommittedBlockHeader.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_COMMITTEDBLOCKHEADER_H
#define SKALED_COMMITTEDBLOCKHEADER_H

#include "BlockProposalHeader.h"

class CommittedBlockHeader : public BlockProposalHeader {

    string thresholdSig;
    ptr<map<uint64_t, string>> decryptedArgKeys = nullptr;

public:
    CommittedBlockHeader(BlockProposal &block, const string &thresholdSig,
                         ptr<map<uint64_t, string>> _decryptedTEKeys);

    explicit CommittedBlockHeader(nlohmann::json &json);

    const ptr<map<uint64_t, string>> &getDecryptedArgKeys() const;

    [[nodiscard]] const string &getThresholdSig() const;

    void addFields(nlohmann::basic_json<> &j) override;
};


#endif //SKALED_COMMITTEDBLOCKHEADER_H
