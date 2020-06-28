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

    @file ConsensusBLSSignature.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_CONSENSUSBLSSIGNATURE_H
#define SKALED_CONSENSUSBLSSIGNATURE_H

// constexpr uint64_t  MAX_BLS_SIGNATURE_SIZE = 64;


#include "BLSSignature.h"
#include "ThresholdSignature.h"

class ConsensusSigShareSet;

class ConsensusBLSSignature : public ThresholdSignature {

    ptr<BLSSignature> blsSig = nullptr;

public:

    ConsensusBLSSignature(
        ptr< string > _sig, block_id _blockID, size_t _totalSigners, size_t _requiredSigners );


    ConsensusBLSSignature( ptr< BLSSignature > _blsSig, block_id _blockID, size_t _totalSigners,
        size_t _requiredSigners );

    std::shared_ptr<std::string> toString();


    uint64_t getRandom();

    ptr<BLSSignature> getBlsSig() const;
};


#endif  // SKALED_CONSENSUSBLSSIGNATURE_H
