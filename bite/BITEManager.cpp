

#include "SkaleCommon.h"
#include "Log.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"
#include "rlp/ParsedEthTransaction.h"

#include "BiteManager.h"
BiteManager::BiteManager( Schain& schain ) : schain( schain ) {}
ConnectionSubStatus BiteManager::verifyAndDecryptProposalTransactions(
    const ptr< BlockProposal >& _proposal ) {
    auto transactions = _proposal->getTransactionList()->getItems();

    CHECK_STATE( transactions );

    try {
        for ( auto& tx : *transactions ) {
            tx->parseAndValidate();
        }
    } catch ( exception& e ) {
        LOG(err, string("Could not parse transaction:") + e.what());
        return ConnectionSubStatus::CONNECTION_ERROR_CANT_PARSE_PROPOSAL_TRANSACTIONS;
    }

    return CONNECTION_OK;
}
