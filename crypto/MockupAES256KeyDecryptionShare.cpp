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

    @file MockupSigShare.cpp
    @author Stan Kladko
    @date 2019
*/


#include "SkaleCommon.h"
#include "Log.h"


#include "thirdparty/json.hpp"

#include "MockupAES256KeyDecryptionShare.h"

MockupAES256KeyDecryptionShare::MockupAES256KeyDecryptionShare( const string& _aes256KeyDecryptionShare, schain_id _schainID, block_id _blockID,
    transaction_index _transactionIndex,
    schain_index _decryptorIndex, size_t _totalDecryptors, size_t _requiredDecryptors )
    : ThresholdAES256KeyDecryptionShare( _schainID, _blockID, _transactionIndex, _decryptorIndex ) {
    CHECK_ARGUMENT( _aes256KeyDecryptionShare != "" );
    CHECK_ARGUMENT( _requiredDecryptors <= _totalDecryptors );
    this->totalDecryptors = _totalDecryptors;
    this->requiredDecryptors = _requiredDecryptors;
    this->aes256DecryptionShare = _aes256KeyDecryptionShare;
}

MockupAES256KeyDecryptionShare::~MockupAES256KeyDecryptionShare() {}

string MockupAES256KeyDecryptionShare::toString() {
    CHECK_STATE( aes256DecryptionShare != "" );
    return aes256DecryptionShare;
}
