#pragma once

#include <vector>
#include <array>
#include <flatbuffers/flatbuffers.h>
#include "crypto/DecryptedAESKeyList.h"
#include "flatb/common_structures_generated.h"
#include "SkaleCommon.h"

namespace BiteAESKeySerializer {

inline std::vector<skale_fb::AesKey> serialize(const DecryptedAESKeyList& decryptedList) {
    static_assert(BITE_AES_KEY_LEN == 32, "FlatBuffer AesKey requires 32-byte AES keys");

    std::vector<skale_fb::AesKey> aesKeysVec;
    for (auto &&[txIdx, keys]: decryptedList.getKeys()) {
        CHECK_STATE(keys);
        for (const auto& key : *keys) {
            auto rawKey = key.getAesKey(); // std::array<uint8_t, BITE_AES_KEY_LEN>
            aesKeysVec.push_back(skale_fb::AesKey{
                static_cast<uint32_t>(txIdx),
                rawKey
            });
        }
    }
    return aesKeysVec;
}

inline void deserialize(const flatbuffers::Vector<const skale_fb::AesKey*>* fbAesKeys,
                        DecryptedAESKeyList& outList) {
    CHECK_STATE(fbAesKeys);

    if (fbAesKeys->empty()) {
        return;
    }

    DecryptedAESKeys  keysForCurrTx;
    transaction_index lastTxIdx = (*fbAesKeys)[0]->transaction_index();

    for (const auto* aesKey : *fbAesKeys) {
        CHECK_STATE(aesKey->data() && aesKey->data()->size() == BITE_AES_KEY_LEN);

        std::array<uint8_t, BITE_AES_KEY_LEN> rawKey{};
        std::memcpy(rawKey.data(), aesKey->data()->data(), BITE_AES_KEY_LEN);

        transaction_index txIdx = aesKey->transaction_index();

        if (txIdx != lastTxIdx) {
            CHECK_STATE(!keysForCurrTx.empty());
            outList.addKeys(lastTxIdx, keysForCurrTx);
            keysForCurrTx.clear();
            lastTxIdx = txIdx;
        }

        keysForCurrTx.emplace_back(rawKey);
    }

    CHECK_STATE(!keysForCurrTx.empty());
    outList.addKeys(lastTxIdx, keysForCurrTx);
}

}  // namespace BiteAESKeySerializer
