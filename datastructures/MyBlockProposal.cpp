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
#include "crypto/AESKeyDecryptionShareList.h"
#include "chains/Schain.h"
#include "Transaction.h"
#include "TransactionList.h"
#include "bite/BiteManager.h"



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

ptr<MyBlockProposal> MyBlockProposal::createMyProposal(
    Schain &_sChain, const block_id &_blockID, const schain_index &_proposerIndex,
    const ptr<TransactionList> &_transactions, u256 _stateRoot, uint64_t _timeStamp,
    uint32_t _timeStampMs, const ptr<CryptoManager> &_cryptoManager) {
    auto proposal = shared_ptr<MyBlockProposal>(new MyBlockProposal(
        _sChain, _blockID, _proposerIndex, _transactions, _stateRoot, _timeStamp, _timeStampMs, _cryptoManager));


#ifdef BITE
    auto failedTransactions =
            _sChain.getBiteManager()->verifyAndCreateMyDecryptionSharesForProposalTransactions(proposal);
    if (!failedTransactions.empty()) {
        LOG(err, "Could not decrypt BITE transactions");
        LOG(err, "Proposing empty transactions instead");
        // could not decrypt proposals, this means something is wrong with the SGX
        // do an empty proposal instead
        // TODO propose non-BITE transactions
        proposal = MyBlockProposal::createMyProposal(_sChain, _blockID,
                                                       _sChain.getSchainIndex(),
                                                       make_shared<TransactionList>(
                                                           make_shared<vector<ptr<Transaction> > >()),
                                                       _stateRoot, _timeStamp, _timeStampMs,
                                                       _cryptoManager);
        BiteManager::parseBITETransactions(proposal, _sChain.getNode()->getCurrentEpochId());
        CHECK_STATE(proposal->getFailedTransactionsRef().empty());
// shares are already set inside verifyAndCreateMyDecryptionSharesForProposalTransactions fucntion
// no need to set them twice
//        proposal->setMyDecryptionShares(make_shared<AESKeyDecryptionShareList>(
//                                                   _blockID, _sChain.getSchainIndex(),
//                                                   _sChain.getSchainIndex()));
    }
#endif



    return proposal;
}


atomic< int64_t > MyBlockProposal::totalObjects( 0 );

MyBlockProposal::~MyBlockProposal() {
    totalObjects--;
}
