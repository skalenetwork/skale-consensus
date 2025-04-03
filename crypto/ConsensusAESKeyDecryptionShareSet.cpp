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


using namespace std;


ConsensusAESKeyDecryptionShareSet::ConsensusAESKeyDecryptionShareSet( block_id _blockId,
    transaction_index _transactionIndex, size_t _totalDecryptors, size_t _requiredDecryptors )
    : ThresholdAESKeyDecryptionShareSet(
          _blockId, _transactionIndex, _totalDecryptors, _requiredDecryptors ) {};

ConsensusAESKeyDecryptionShareSet::~ConsensusAESKeyDecryptionShareSet() = default;


ptr< DecryptedAESKey > ConsensusAESKeyDecryptionShareSet::mergeAESKey() {
    LOCK( decryptionSharesLock )

    CHECK_STATE( isEnough() );

    uint processedShares = 0;
    libBLS::TEDecryptSet decryptSet( requiredDecryptors, totalDecryptors );

    for ( auto&& item : decryptionShares ) {
        CHECK_STATE( item.second );
        decryptSet.addDecryptShare( *item.second->getTEDecryptionShare() );
        processedShares++;
        if ( processedShares == requiredDecryptors ) {
            break;
        }
    }
    CHECK_STATE( decryptSet.canMerge() );

     std::array< uint8_t, BITE_AES_KEY_LEN > aesKey;


    return std::make_shared< DecryptedAESKey >(aesKey);
}

bool ConsensusAESKeyDecryptionShareSet::isEnough() {
    {
        LOCK( decryptionSharesLock )
        return ( decryptionShares.size() >= requiredDecryptors );
    }
}


bool ConsensusAESKeyDecryptionShareSet::addDecryptionShare(
    const ptr< ThresholdAESKeyDecryptionShare >& _decryptionShare ) {
    CHECK_ARGUMENT( _decryptionShare );

    LOCK( decryptionSharesLock )

    if ( isEnough() )
        return false;

    if ( decryptionShares.count( ( uint64_t ) _decryptionShare->getDecryptorIndex() ) > 0 ) {
        return false;
    }

    auto ds = dynamic_pointer_cast< ConsensusAESKeyDecryptionShare >( _decryptionShare );

    CHECK_STATE( ds );

    decryptionShares[( uint64_t ) _decryptionShare->getDecryptorIndex()] = ds;

    return true;
}
