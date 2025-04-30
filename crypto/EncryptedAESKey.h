#pragma once

#include <memory>
#include <array>
#include <cstdint>

using namespace std;


class EncryptedAESKey {
   std::shared_ptr<std::array<std::uint8_t, BITE_ENCRYPTED_AES_KEY_LEN>> key;

public:
   explicit EncryptedAESKey(shared_ptr<array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN>> key);

   [[nodiscard]] shared_ptr<array<uint8_t, BITE_ENCRYPTED_AES_KEY_LEN>> getKey() const {
      return key;
   }

   [[nodiscard]] shared_ptr<string> toHex() const;
};