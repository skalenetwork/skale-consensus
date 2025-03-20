#include "SkaleCommon.h"
#include "Log.h"

#include "bls_include.h"
#include "bls/bls.h"


#include "MockupAESKeyDecryptionShare.h"

#include "DecryptedAESKey.h"

#include "DecryptedAESKey.h"
#include "MockupAESKeyDecryptionShareSet.h"

using namespace std;

MockupAESKeyDecryptionShareSet::MockupAESKeyDecryptionShareSet(
    block_id _blockId, size_t _totalDecryptors, size_t _requiredDecryptors )
    : ThresholdAESKeyDecryptionShareSet( _blockId, _totalDecryptors, _requiredDecryptors ) {
    CHECK_ARGUMENT( _requiredDecryptors > 0 );
    CHECK_ARGUMENT( _requiredDecryptors <= totalDecryptors );

    totalObjects++;
}

MockupAESKeyDecryptionShareSet::~MockupAESKeyDecryptionShareSet() {
    totalObjects--;
}

ptr< DecryptedAESKey > MockupAESKeyDecryptionShareSet::mergeAESKey() {
    string h( "" );

    LOCK( decryptionSharesLock )

    for ( auto&& item : decryptionShares ) {
        CHECK_STATE( item.second );
        if ( h.empty() ) {
            h = item.second->toString();
        }
    }
    CHECK_STATE( !h.empty() );

    CHECK_STATE(h.size() == AES_KEY_LEN * 2);

    return make_shared< DecryptedAESKey >( h, blockId, totalDecryptors, requiredDecryptors );
}

bool MockupAESKeyDecryptionShareSet::isEnough() {
    LOCK( decryptionSharesLock )
    return ( decryptionShares.size() >= requiredDecryptors );
}


bool MockupAESKeyDecryptionShareSet::addDecryptionShare(
    const ptr< ThresholdAESKeyDecryptionShare >& _decryptionShare ) {
    CHECK_ARGUMENT( _decryptionShare );

    LOCK( decryptionSharesLock )

    if ( isEnough() )
        return false;

    if ( decryptionShares.count( ( uint64_t ) _decryptionShare->getDecryptorIndex() ) > 0 ) {
        return false;
    }

    auto ds = dynamic_pointer_cast< MockupAESKeyDecryptionShare >( _decryptionShare );

    CHECK_STATE( ds );

    decryptionShares[( uint64_t ) _decryptionShare->getDecryptorIndex()] = ds;

    return true;
}
