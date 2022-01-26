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

#include "ArgumentDecryptionShare.h"

ArgumentDecryptionShare::ArgumentDecryptionShare(const block_id & _blockId, const uint64_t _totalShares,
                                                 const te_share_index &_schainIndex, const string & _data) :
        data( _data ), blockId( _blockId ), totalShares(_totalShares ), schainIndex(_schainIndex) {

    CHECK_ARGUMENT(!_data.empty() );
    CHECK_ARGUMENT(_schainIndex <= _totalShares );
    CHECK_ARGUMENT( _blockId > 0);

    if ( _data.size() < 3) {
        BOOST_THROW_EXCEPTION(ParsingException("Decryption share too short:" +
         to_string( _data.size()), __CLASS_NAME__));
    }
}

block_id ArgumentDecryptionShare::getBlockId() const {
    return blockId;
}

uint64_t ArgumentDecryptionShare::getTotalShares() const {
    return totalShares;
}

te_share_index ArgumentDecryptionShare::getSchainIndex() const {
    return schainIndex;
}