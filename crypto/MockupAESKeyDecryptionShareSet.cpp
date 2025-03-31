#include "SkaleCommon.h"
#include "Log.h"

#include "bls_include.h"
#include "MockupAESKeyDecryptionShare.h"
#include "DecryptedAESKey.h"
#include "MockupAESKeyDecryptionShareSet.h"

#include <network/Utils.h>

using namespace std;

MockupAESKeyDecryptionShareSet::MockupAESKeyDecryptionShareSet(
    block_id _blockId, transaction_index _transactionIndex, size_t _totalDecryptors, size_t _requiredDecryptors )
    : AESKeyDecryptionShareSet( _blockId, _transactionIndex, _totalDecryptors, _requiredDecryptors ) {
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
    CHECK_STATE(h.size() == BITE_ENCRYPTED_AES_KEY_LEN * 2);

    std::array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN> encryptedKey{};
    std::array<uint8_t, BITE_AES_KEY_LEN> decryptedKey{};

    Utils::cArrayFromHex(h, encryptedKey.data(), BITE_ENCRYPTED_AES_KEY_LEN);

    CHECK_STATE(encryptedKey.at(0) == 1);

    std::copy_n(encryptedKey.begin() + 1, BITE_AES_KEY_LEN, decryptedKey.begin());

    return make_shared< DecryptedAESKey >(decryptedKey);
}

bool MockupAESKeyDecryptionShareSet::isEnough() {
    LOCK( decryptionSharesLock )
    return ( decryptionShares.size() >= requiredDecryptors );
}


bool MockupAESKeyDecryptionShareSet::addDecryptionShare(
    const ptr< AESKeyDecryptionShare >& _decryptionShare ) {
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
