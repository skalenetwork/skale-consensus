

#include <chains/Schain.h>

#include "SkaleCommon.h"
#include "Log.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/CommittedBlock.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"
#include "rlp/ParsedEthTransaction.h"

#include "BiteManager.h"
BiteManager::BiteManager( Schain& _schain ) : schain( _schain ) {
    doRealCrypto = _schain.getNode()->verifyRealSignatures();
}


ConnectionSubStatus BiteManager::verifyAndDecryptProposalTransactions(
    const ptr<BlockProposal> &_proposal) {
    auto transactions = _proposal->getTransactionList()->getItems();

    CHECK_STATE( transactions );


    std::map< transaction_index, ptr< BITEDataField >> biteDataFields;

    try {
        transaction_index index = 0;
        for ( auto& tx : *transactions ) {
            tx->parseAndValidate();
            if ( tx->getBITEDataField() ) {
                biteDataFields[index] = tx->getBITEDataField();
            }
            index = index + 1;
        }
    } catch ( exception& e ) {
        LOG(err, string("Could not parse transaction:") + e.what());
        return ConnectionSubStatus::CONNECTION_ERROR_CANT_PARSE_PROPOSAL_TRANSACTIONS;
    }

    return CONNECTION_OK;
}


void BiteManager::verifyAndDecryptBlockTransactions(
    const ptr<CommittedBlock> &_block) {
    auto transactions = _block->getTransactionList()->getItems();

    CHECK_STATE( transactions );

    try {
        for ( auto& tx : *transactions ) {
            tx->parseAndValidate();
        }
    } CATCH_LOG_AND_RETHROW_ANY_EXCEPTION(err, "Could not parse transaction");



}