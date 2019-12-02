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

#include "../SkaleCommon.h"
#include "../crypto/SHAHash.h"
#include "../chains/Schain.h"
#include "ReceivedBlockProposal.h"

ReceivedBlockProposal::ReceivedBlockProposal(Schain &_sChain, const block_id &_blockID,
                                             const schain_index &_proposerIndex,
                                             const ptr<TransactionList> &_transactions,
                                             const uint64_t &_timeStamp,
                                             const uint32_t &_timeStampMs,
                                             ptr<string> _hash,
                                             ptr<string> _signature) : BlockProposal(
        _sChain.getSchainID(), _sChain.getNodeIDByIndex(_proposerIndex), _blockID,
        _proposerIndex, _transactions,
        _timeStamp, _timeStampMs, _signature, nullptr) {
    this->hash = SHAHash::fromHex(_hash);
    this->signature = _signature;
    totalObjects++;
}

ReceivedBlockProposal::ReceivedBlockProposal(Schain &_sChain, const block_id &_blockID,
                                             const ptr<TransactionList> &_transactions,
                                             const uint64_t &_timeStamp,
                                             const uint32_t &_timeStampMs) : BlockProposal(
        _sChain.getSchainID(), 0, _blockID,
        0, _transactions, _timeStamp, _timeStampMs, ptr<string>(), ptr<CryptoManager>()) {
    calculateHash();
    this->signature = make_shared<string>("EMPTY");
    totalObjects++;
}





atomic<uint64_t>  ReceivedBlockProposal::totalObjects(0);

ReceivedBlockProposal::~ReceivedBlockProposal() {
    totalObjects--;
}


