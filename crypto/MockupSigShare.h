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

    @file MockupSigShare.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_MOCKUPSIGSHARE_H
#define SKALED_MOCKUPSIGSHARE_H


#include "BLSSigShare.h"
#include "ThresholdSigShare.h"


class MockupSigShare : public ThresholdSigShare {
    uint64_t totalSigners = 0;
    uint64_t requiredSigners = 0;

    string sigShare;

public:
    MockupSigShare( const string& _sigShare, schain_id _schainID, block_id _blockID,
        schain_index _signerIndex, size_t _totalSigners, size_t _requiredSigners );

    string toString() override;

    ~MockupSigShare() override;
};


#endif  // SKALED_MockupSIGSHARE_H
