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

    @file ConsensusBLSSigShare.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_CONSENSUSBLSSIGSHARE_H
#define SKALED_CONSENSUSBLSSIGSHARE_H


#include "BLSSigShare.h"
#include "ThresholdSigShare.h"

namespace libff {
class alt_bn128_G1;
}

class ConsensusBLSSigShare : public ThresholdSigShare {


    ptr<BLSSigShare> blsSigShare;

public:




    ConsensusBLSSigShare(const ptr<BLSSigShare>& _sig, schain_id _schainId, block_id _blockID);


    ConsensusBLSSigShare(const ptr<string>& _sigShare, schain_id _schainID, block_id _blockID, schain_index _signerIndex,
                         uint64_t _totalSigners, uint64_t _requiredSigners);


    ptr< BLSSigShare > getBlsSigShare() const;

    virtual ptr<std::string> toString();

    virtual ~ConsensusBLSSigShare();
};


#endif  // SKALED_CONSENSUSBLSSIGSHARE_H
