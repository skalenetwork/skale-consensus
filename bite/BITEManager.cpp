

#include "SkaleCommon.h"
#include "Log.h"

#include "BiteManager.h"
BiteManager::BiteManager( Schain& schain ) : schain( schain ) {
}
ConnectionSubStatus BiteManager::verifyAndDecryptProposalTransactions(
    const ptr< BlockProposal >& _proposal ) {
    return CONNECTION_ERROR_UNKNOWN_SCHAIN_ID;
}
