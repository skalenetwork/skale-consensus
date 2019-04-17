/*
    Copyright (C) 2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file ReceivedSigSharesDatabase.h
    @author Stan Kladko
    @date 2019
*/

#pragma once


class SigShareSet;
class BLSSignature;
class Schain;

class ReceivedSigSharesDatabase : Agent {


    recursive_mutex sigShareDatabaseMutex;

    map<block_id, ptr<SigShareSet>> sigShareSets;

    map<block_id, ptr<BLSSignature>> blockSignatures;


    ptr<SigShareSet> getSigShareSet(block_id _blockID);

    ptr<BLSSignature> getBLSSignature(block_id _blockId);

public:



    ReceivedSigSharesDatabase(Schain &_sChain);

    bool addSigShare(ptr<BLSSigShare> _proposal);

    void mergeAndSaveBLSSignature(block_id _blockId);

    bool isTwoThird(block_id _blockID);
};



