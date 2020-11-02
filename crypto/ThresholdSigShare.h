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

    @file ThresholdSigShare.h
    @author Stan Kladko
    @date 2019
*/
#ifndef SKALED_THRESHOLDSIGSHARE_H
#define SKALED_THRESHOLDSIGSHARE_H

class  BLAKE3Hash;

#include "BLSSigShare.h"
#include "SkaleCommon.h"

class ThresholdSigShare {

protected:
    schain_id schainId;
    block_id blockId;
    schain_index signerIndex;

public:

    block_id getBlockId() const;

    ThresholdSigShare(const schain_id &schainId, const block_id &blockId, const schain_index &_signerIndex);

    virtual string toString() = 0;

    virtual ~ThresholdSigShare();

    schain_index getSignerIndex() const;
    ptr<BLAKE3Hash> computeHash();

};


#endif //SKALED_THRESHOLDSIGSHARE_H
