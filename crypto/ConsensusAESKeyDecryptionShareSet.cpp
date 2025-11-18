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
#include <memory>
#include "Log.h"
#include "SkaleCommon.h"
#include "libBLS/threshold_encryption/TEDecryptSet.h"

#include "ConsensusAESKeyDecryptionShare.h"
#include "ConsensusAESKeyDecryptionShareSet.h"
#include "crypto/AESKeyDecryptionShare.h"
#include "threshold_encryption/ThresholdEncryption.h"


using namespace std;


ConsensusAESKeyDecryptionShareSet::ConsensusAESKeyDecryptionShareSet( block_id _blockId,
    transaction_index _transactionIndex, size_t numCiphertexts, size_t _totalDecryptors, size_t _requiredDecryptors )
    : AESKeyDecryptionShareSet( _blockId, _transactionIndex ) {
        decryptionSets.reserve( numCiphertexts );
        for ( size_t i = 0; i < numCiphertexts; i++ ) {
            decryptionSets.emplace_back(libBLS::TEDecryptSet( _requiredDecryptors, _totalDecryptors ));
        }
    };

ConsensusAESKeyDecryptionShareSet::~ConsensusAESKeyDecryptionShareSet() = default;

ptr< DecryptedAESKeys > ConsensusAESKeyDecryptionShareSet::verifyAndMergeAESKeys(EncryptedAESKeys& _encryptedAESKeys) {
    CHECK_STATE( _encryptedAESKeys.size() == decryptionSets.size() );
    LOCK( decryptionSharesLock )

    bool validateCiphertext = false;
    auto decryptedKeys = make_shared< DecryptedAESKeys >();
    for ( size_t i = 0; i < _encryptedAESKeys.size(); i++ ) {
        auto cipheredKey = libBLS::CipheredKey::fromBytes( _encryptedAESKeys.at(i).data(), validateCiphertext );
        // Checks if decryption set can be merged & merges if so
        libBLS::AES256Key aesKey = libBLS::ThresholdEncryption::combineShares( cipheredKey, decryptionSets.at(i) );
        decryptedKeys->push_back( DecryptedAESKey( aesKey ) );
    }

    return decryptedKeys;
}

bool ConsensusAESKeyDecryptionShareSet::isEnough() {
    {
        LOCK( decryptionSharesLock )
        return decryptionSets.at(0).canMerge();
    }
}


bool ConsensusAESKeyDecryptionShareSet::addDecryptionShares(
    const ptr< AESKeyDecryptionShares >& _decryptionShares ) {
    CHECK_ARGUMENT( _decryptionShares );
    CHECK_STATE( _decryptionShares->size() == decryptionSets.size() );

    LOCK( decryptionSharesLock )
    
    for ( size_t i = 0; i < decryptionSets.size(); i++ ) {
        ptr< AESKeyDecryptionShare > val = _decryptionShares->at(i);
        auto ds = std::dynamic_pointer_cast< ConsensusAESKeyDecryptionShare >( val );
        CHECK_STATE( ds );

        try {
            decryptionSets.at(i).addDecryptShare( *ds->getTEDecryptionShare() );
        }
        catch ( const std::exception& e ) {
            CONS_LOG( warn, "Failed to add decryption share: " << e.what() );
            return false;
        }
    }
    // count as a single share added (we count all shares for current tx as one)
    totalObjects.fetch_add( 1 );

    return true;
}
