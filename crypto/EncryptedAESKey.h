#pragma once

#include <memory>
#include <array>
#include <cstdint>
#include "datastructures/SmallVector.h"
#include "bite/Constants.h"

using namespace std;

class EncryptedAESKey {
   array< uint8_t, BITE_ENCRYPTED_AES_KEY_LEN> regularTxKey;

public:
   explicit EncryptedAESKey(array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN> key);

   [[nodiscard]] array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN> data() const {
      return regularTxKey;
   }

   [[nodiscard]] shared_ptr<string> toHex() const;
};

using EncryptedAESKeys = small_vector< EncryptedAESKey >;