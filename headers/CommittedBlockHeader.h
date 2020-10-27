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

public:
    CommittedBlockHeader(BlockProposal &block, const string &thresholdSig);

    CommittedBlockHeader(rapidjson::Document &json);

    const string &getThresholdSig() const;

    void addFields(rapidjson::Writer<rapidjson::StringBuffer> &_j) override;
};


#endif //SKALED_COMMITTEDBLOCKHEADER_H
