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

    @file ReceivedDASigSharesDatabase.h
    @author Stan Kladko
    @date 2019
*/

#pragma once


#include "Agent.h"

class ConsensusSigShareSet;
class ConsensusBLSSignature;
class Schain;
class ConsensusBLSSigShare;
class ThresholdSigShareSet;
class ThresholdSignature;
class ThresholdSigShare;

#include "CacheLevelDB.h"

class DASigShareDB : public CacheLevelDB {
    recursive_mutex sigShareMutex;

public:
    explicit DASigShareDB(
        Schain* _sChain, string& _dirName, string& _prefix, node_id _nodeId, uint64_t _maxDBSize );

    ptr< DAProof > addAndMergeSigShareAndVerifySig(
        const ptr< ThresholdSigShare >& _sigShare, const ptr< BlockProposal >& _proposal );

    const string& getFormatVersion();
};
