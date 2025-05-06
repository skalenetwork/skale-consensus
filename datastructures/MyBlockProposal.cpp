/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file MyBlockProposal.cpp
    @author Stan Kladko
    @date 2018
*/


#include "SkaleCommon.h"
#include "Log.h"


#include "crypto/CryptoManager.h"
#include "chains/Schain.h"
#include "utils/Time.h"
#include "Transaction.h"

#include "MyBlockProposal.h"


MyBlockProposal::MyBlockProposal( Schain& _sChain, const block_id& _blockID,
    const schain_index& _proposerIndex, const ptr< TransactionList >& _transactions,
    u256 _stateRoot, uint64_t _timeStamp, uint32_t _timeStampMs,
    const ptr< CryptoManager >& _cryptoManager )
    : BlockProposal( _sChain.getSchainID(), _sChain.getNodeIDByIndex( _proposerIndex ), _blockID,
          _proposerIndex, _transactions, _stateRoot, _timeStamp, _timeStampMs, "",
          _cryptoManager ) {
    CHECK_STATE( _transactions );
    CHECK_ARGUMENT( _cryptoManager );
    totalObjects++;
};


atomic< int64_t > MyBlockProposal::totalObjects( 0 );

MyBlockProposal::~MyBlockProposal() {
    totalObjects--;
}
