#pragma once

#ifndef SKALED_BITETRANSACTION_H
#define SKALED_BITETRANSACTION_H

#include <system_error>

#include <string_view>
#include <vector>
#include <vector>
#include <cstddef>      // For std::byte
#include <algorithm>    // For std::equal
#include <iostream>
#include <memory>
#include <variant>


#include <SkaleCommon.h>

#include "BITEKey.h"

class BITETransaction {

    vector<ptr<BITEKey>> encryptedKey;

    static constexpr string_view BITE_MAGIC_AS_STRING = "F3A9C7B1E4D5F28C7B1E9A3F5D2C8B0";


    static constexpr uint64_t MAGIC_SIZE = 16;
    static constexpr uint64_t ENCRYPTED_KEY_SIZE = 16;

    static constexpr uint8_t BITE_MAGIC_AS_BYTE_ARRAY[MAGIC_SIZE] = {0xF3, 0xA9, 0xC7, 0xB1, 0xE4, 0xD5, 0xF2, 0x8C, 0x7B, 0x1E,
                                                                     0x9A, 0x3F, 0x5D, 0x2C, 0x8B, 0x0};

    static bool isBITETransaction(const std::vector<uint8_t> & _dataField) {
        return _dataField.size() >= MAGIC_SIZE &&
               std::equal(BITE_MAGIC_AS_BYTE_ARRAY, BITE_MAGIC_AS_BYTE_ARRAY + MAGIC_SIZE, _dataField.begin());
    };

public:

    BITETransaction(const std::vector<uint8_t> _dataField) {
    }

    static ptr<BITETransaction> createBiteTransactionIfMatch(const std::vector<uint8_t>& _dataField) {
        if (isBITETransaction(_dataField)) {
            return std::make_shared<BITETransaction>(_dataField);
        }
        return nullptr;
    }
};
