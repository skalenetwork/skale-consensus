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

    @file ThresholdSignature.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_THRESHOLDSIGNATURE_H
#define SKALED_THRESHOLDSIGNATURE_H


#include "BLSSignature.h"

class ThresholdSignature {

protected:
    block_id blockId;

public:
    ThresholdSignature(const block_id &blockId);

    block_id getBlockId() const;

    virtual std::shared_ptr<std::string> toString() = 0;

    virtual uint64_t getRandom() = 0;

    virtual ~ThresholdSignature();

};


#endif //SKALED_THRESHOLDSIGNATURE_H
