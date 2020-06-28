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

    @file SigDB.cpp
    @author Stan Kladko
    @date 2019
*/


#include "SkaleCommon.h"
#include "Log.h"

#include "crypto/ConsensusBLSSignature.h"
#include "crypto/ConsensusBLSSigShare.h"
#include "datastructures/CommittedBlock.h"

#include "SigDB.h"


SigDB::SigDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize) :
        CacheLevelDB(_sChain, _dirName, _prefix, _nodeId, _maxDBSize, false) {}


const string SigDB::getFormatVersion() {
    return "1.0";
}




void SigDB::addSignature(block_id _blockId, ptr<ThresholdSignature> _sig) {
    CHECK_ARGUMENT(_sig);
    auto key = createKey(_blockId);
    CHECK_STATE(key);
    if (readString(*key) == nullptr)
        writeString(*key, *_sig->toString());
}


