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

    @file ThresholdSigShareSet.h
    @author Stan Kladko
    @date 2019
*/
#ifndef SKALED_THRESHOLDSIGSHARESET_H
#define SKALED_THRESHOLDSIGSHARESET_H

class ThresholdSignature;
class ThresholdSigShare;

class ThresholdSigShareSet {
public:
    ThresholdSigShareSet(const block_id _blockId, uint64_t _totalSigners, uint64_t _requiredSigners);

protected:
    block_id blockId;
    uint64_t totalSigners;
    uint64_t requiredSigners;
    recursive_mutex m;
    static atomic<int64_t>  totalObjects;

public:
    virtual ~ThresholdSigShareSet();

    static int64_t getTotalObjects();

    virtual ptr<ThresholdSignature> mergeSignature() = 0;

    virtual bool isEnough() = 0;

    virtual bool addSigShare(const ptr<ThresholdSigShare>& _sigShare) = 0;

};


#endif //SKALED_THRESHOLDSIGSHARESET_H
