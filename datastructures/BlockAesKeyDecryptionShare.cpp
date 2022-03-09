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

    @file BlockDecryptionShare.cpp
    @author Stan Kladko
    @date 2019
*/
#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/ParsingException.h"

#include "BlockAesKeyDecryptionShare.h"

BlockAesKeyDecryptionShares::BlockAesKeyDecryptionShares(const block_id & _blockId, const uint64_t _totalShares,
                                                         const te_share_index &_schainIndex,
                                                         ptr<map<uint64_t, string>> _data) :
        data( _data ), blockId( _blockId ), totalShares(_totalShares ), schainIndex(_schainIndex) {

    CHECK_ARGUMENT(_data);
    CHECK_ARGUMENT(_data->size() >0 );
    CHECK_ARGUMENT(_schainIndex <= _totalShares );
    CHECK_ARGUMENT( _blockId > 0);
}

block_id BlockAesKeyDecryptionShares::getBlockId() const {
    return blockId;
}

uint64_t BlockAesKeyDecryptionShares::getTotalShares() const {
    return totalShares;
}

te_share_index BlockAesKeyDecryptionShares::getSchainIndex() const {
    return schainIndex;
}

const ptr<map<uint64_t, string>> &BlockAesKeyDecryptionShares::getData() const {
    return data;
}
