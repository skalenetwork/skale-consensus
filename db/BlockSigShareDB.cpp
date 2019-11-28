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

    @file BlockSigShareDB.cpp
    @author Stan Kladko
    @date 2019
*/


#include "../SkaleCommon.h"
#include "../Log.h"
#include "../crypto/SHAHash.h"
#include "../chains/Schain.h"
#include "../exceptions/InvalidStateException.h"
#include "../datastructures/CommittedBlock.h"
#include "../crypto/ThresholdSigShare.h"

#include "BlockSigShareDB.h"


BlockSigShareDB::BlockSigShareDB(string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize)
        : LevelDB(_dirName, _prefix,
                  _nodeId, _maxDBSize) {
}


bool
BlockSigShareDB::checkAndSaveShare(ptr<ThresholdSigShare> _sigShare) {
    CHECK_ARGUMENT(_sigShare != nullptr);
    auto sigShareString = _sigShare->toString();
    auto count = writeStringToBlockSet("", *sigShareString, _sigShare->getBlockId(),
            _sigShare->getSignerIndex());
    return isEnough(count);
}




const string BlockSigShareDB::getFormatVersion() {
    return "1.0";
}






