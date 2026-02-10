#pragma once


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#include <boost/container/flat_map.hpp>
#include "datastructures/SmallVector.h"
#pragma GCC diagnostic pop

#include "SkaleCommon.h"

class BLAKE3Hash;


class AESKeyDecryptionShare {
protected:
    schain_index decryptorIndex = 0;
    bool decryptionFailed = true;
public:


    AESKeyDecryptionShare( const schain_index& _decryptorIndex, const bool _decryptionFailed );

    virtual string toString() = 0;


    [[nodiscard]] schain_index getDecryptorIndex() const {
        return decryptorIndex;
    }

    [[nodiscard]] bool isDecryptionFailed() const {
        return decryptionFailed;
    }

    virtual ~AESKeyDecryptionShare();

    BLAKE3Hash computeHash();
};

using AESKeyDecryptionShares = small_vector<ptr<AESKeyDecryptionShare>>;