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

    @file ReceivedSigSharesDatabase.h
    @author Stan Kladko
    @date 2019
*/

#pragma once



#include "../Agent.h"

class SigShareSet;
class ConsensusBLSSignature;
class Schain;
class ConsensusBLSSigShare;

class ReceivedSigSharesDatabase : Agent {

    size_t getTotalSignersCount();

    size_t getRequiredSignersCount();

    recursive_mutex sigShareDatabaseMutex;

    map<block_id, ptr<SigShareSet>> sigShareSets;

    map<block_id, ptr<ConsensusBLSSignature>> blockSignatures;


    ptr<SigShareSet> getSigShareSet(block_id _blockID);

    ptr<ConsensusBLSSignature> getBLSSignature(block_id _blockId);

public:



    explicit ReceivedSigSharesDatabase(Schain &_sChain);

    bool addSigShare(ptr<ConsensusBLSSigShare> _proposal);

    void mergeAndSaveBLSSignature(block_id _blockId);

    bool isTwoThird(block_id _blockID);
};



