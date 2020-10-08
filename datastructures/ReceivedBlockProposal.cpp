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

    @file ReceivedBlockProposal.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "datastructures/TransactionList.h"
#include "crypto/SHAHash.h"
#include "chains/Schain.h"
#include "ReceivedBlockProposal.h"

ReceivedBlockProposal::ReceivedBlockProposal(Schain &_sChain, const block_id &_blockID,
                                             const schain_index &_proposerIndex,
                                             const ptr<TransactionList> &_transactions, u256 _stateRoot,
                                             const uint64_t &_timeStamp,
                                             const uint32_t &_timeStampMs, ptr<string> _hash, ptr<string> _signature) : BlockProposal(
        _sChain.getSchainID(), _sChain.getNodeIDByIndex(_proposerIndex), _blockID,
        _proposerIndex, _transactions, _stateRoot,
        _timeStamp, _timeStampMs, _signature, nullptr) {

    CHECK_ARGUMENT(_transactions);
    CHECK_ARGUMENT(_hash);
    CHECK_ARGUMENT(_signature);


    this->hash = SHAHash::fromHex(_hash);


    this->signature = _signature;
    totalObjects++;
}

ReceivedBlockProposal::ReceivedBlockProposal(Schain &_sChain, const block_id &_blockID, const uint64_t &_timeStamp,
                                             const uint32_t &_timeStampMs, u256 _stateRoot) : BlockProposal(
        _sChain.getSchainID(), 0, _blockID,
        0, make_shared<TransactionList>(make_shared<vector<ptr<Transaction >>>()), _stateRoot, _timeStamp, _timeStampMs,
        make_shared<string>("EMPTY"), ptr<CryptoManager>()) {
    calculateHash();
    totalObjects++;
}

atomic<int64_t>  ReceivedBlockProposal::totalObjects(0);

ReceivedBlockProposal::~ReceivedBlockProposal() {
    totalObjects--;
}


