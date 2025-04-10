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

    ptr< DecryptedAESKey > MockupAESKeyDecryptionShareSet::verifyAndMergeAESKey() {
        string h( "" );

        READ_LOCK( decryptionSharesLock )

        CHECK_STATE(decryptionShares.size() >= requiredDecryptors);


        //check all shares are the same

        std::string first;
        for (auto&& item : decryptionShares) {
            CHECK_STATE(item.second);
            if (first.empty()) {
                first = item.second->toString();
            } else {
                CHECK_STATE(item.second->toString() == first);  // Add this line
            }
        }
        CHECK_STATE( !first.empty() );
        CHECK_STATE(first.size() == BITE_ENCRYPTED_AES_KEY_LEN * 2);

        std::array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN> encryptedKey{};
        std::array<uint8_t, BITE_AES_KEY_LEN> decryptedKey{};

        Utils::cArrayFromHex(first, encryptedKey.data(), BITE_ENCRYPTED_AES_KEY_LEN);

        std::copy_n(encryptedKey.begin(), BITE_AES_KEY_LEN, decryptedKey.begin());

        return make_shared< DecryptedAESKey >(decryptedKey);
    }

    bool MockupAESKeyDecryptionShareSet::isEnough() {
        READ_LOCK( decryptionSharesLock )
        return ( decryptionShares.size() >= requiredDecryptors );
    }

    bool MockupAESKeyDecryptionShareSet::isEnoughUnsafe() {
        return ( decryptionShares.size() >= requiredDecryptors );
    }


    bool MockupAESKeyDecryptionShareSet::addDecryptionShare(
        const ptr< AESKeyDecryptionShare >& _decryptionShare ) {
        CHECK_ARGUMENT( _decryptionShare );

        WRITE_LOCK( decryptionSharesLock )

        if ( isEnoughUnsafe() )
            return false;

        if ( decryptionShares.count( ( uint64_t ) _decryptionShare->getDecryptorIndex() ) > 0 ) {
            return false;
        }

        auto ds = dynamic_pointer_cast< MockupAESKeyDecryptionShare >( _decryptionShare );

        CHECK_STATE( ds );

        decryptionShares[( uint64_t ) _decryptionShare->getDecryptorIndex()] = ds;

        return true;
    }

