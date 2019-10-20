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

    @file CryptoManager.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_CRYPTOMANAGER_H
#define SKALED_CRYPTOMANAGER_H


#include "openssl/ec.h"

class Schain;
class SHAHash;
class ConsensusBLSSigShare;
class ThresholdSigShareSet;
class ThresholdSigShare;
class CryptoManager {

private:
    Schain* sChain;

    static const char* insecureTestECDSAKey;

    EC_KEY* ecdsaKey;


    void init();

public:

    CryptoManager(); // for testing

    CryptoManager(Schain& sChain);

    ptr<string> sign(ptr<SHAHash> _hash);


    Schain *getSchain() const;

    ptr<ThresholdSigShare> sign(ptr<SHAHash> _hash, block_id _blockId);

    ptr<ThresholdSigShareSet> createSigShareSet(block_id _blockId, size_t _totalSigners, size_t _requiredSigners );

    ptr<ThresholdSigShare> createSigShare(ptr<string> _sigShare, schain_id _schainID, block_id _blockID, node_id _signerNodeID,
            schain_index _signerIndex, size_t _totalSigners, size_t _requiredSigners);
};


#endif //SKALED_CRYPTOMANAGER_H
