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

    @file SigShareSet.cpp
    @author Stan Kladko
    @date 2019
*/



#include "bls_include.h"
#include <oids.h>
#include "Log.h"
#include "SkaleCommon.h"
#include "libBLS/threshold_encryption/TEDecryptSet.h"


#include "ConsensusAESKeyDecryptionShare.h"
#include "ConsensusAESKeyDecryptionShareSet.h"
#include "DecryptedAESKey.h"
#include "EncryptedAESKey.h"
#include "threshold_encryption/ThresholdEncryption.h"


using namespace std;


ConsensusAESKeyDecryptionShareSet::ConsensusAESKeyDecryptionShareSet( block_id _blockId,
    transaction_index _transactionIndex, size_t _totalDecryptors, size_t _requiredDecryptors )
    : AESKeyDecryptionShareSet( _blockId, _transactionIndex ), 
    decryptionShares(_requiredDecryptors, _totalDecryptors) {};

ConsensusAESKeyDecryptionShareSet::~ConsensusAESKeyDecryptionShareSet() = default;

ptr< DecryptedAESKey > ConsensusAESKeyDecryptionShareSet::verifyAndMergeAESKey(ptr<EncryptedAESKey> _encryptedAESKey) {
    LOCK( decryptionSharesLock )

    bool validateCiphertext = false;
    auto cipheredKey = libBLS::CipheredKey::fromBytes( *_encryptedAESKey->getKey(), validateCiphertext );

    // Checks if decryption set can be merged & merges if so
    libBLS::AES256Key aesKey = libBLS::ThresholdEncryption::combineShares( cipheredKey, decryptionShares);

    return make_shared< DecryptedAESKey >( aesKey );
}

bool ConsensusAESKeyDecryptionShareSet::isEnough() {
    {
        LOCK( decryptionSharesLock )
        return decryptionShares.canMerge();
    }
}


bool ConsensusAESKeyDecryptionShareSet::addDecryptionShare(
    const ptr< AESKeyDecryptionShare >& _decryptionShare ) {
    CHECK_ARGUMENT( _decryptionShare );

    LOCK( decryptionSharesLock )
    
    auto ds = dynamic_pointer_cast< ConsensusAESKeyDecryptionShare >( _decryptionShare );
    CHECK_STATE( ds );

    try {
        decryptionShares.addDecryptShare( *ds->getTEDecryptionShare() );
        totalObjects.fetch_add( 1 );
    }
    catch ( const std::exception& e ) {
        LOG( warn, "Failed to add decryption share: " << e.what() );
        return false;
    }

    return true;
}
