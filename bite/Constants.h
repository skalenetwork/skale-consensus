#pragma once
#include <string_view>
#include <cstdint>
#include "libBLS/threshold_encryption/ThresholdEncryption.h"

constexpr uint64_t BITE_CHAIN_ID = 0xD1D2D3;
constexpr std::string_view BITE_CHAIN_ID_AS_STRING = "D1D2D3";
constexpr uint8_t BITE_CHAIN_ID_AS_BYTE_ARRAY[3] = {0xD1, 0xD2, 0xD3};

constexpr uint64_t ADDRESS_SIZE = 20;
constexpr std::string_view BITE_ADDRESS_AS_STRING = "42495445204D452049274D20454E435259505444";
constexpr uint8_t BITE_ADDRESS_AS_BYTE_ARRAY[ADDRESS_SIZE] = {0x42, 0x49, 0x54, 0x45, 0x20, 0x4D, 0x45, 0x20,
    0x49, 0x27, 0x4D, 0x20, 0x45, 0x4E, 0x43, 0x52,
    0x59, 0x50, 0x54, 0x44};

#ifdef BITE2
constexpr uint64_t BITE_FUNCTION_SELECTOR_SIZE_BYTES = 4;
// TODO - update to the actual function selector once decided
constexpr uint8_t BITE_FUNCTION_SELECTOR_AS_BYTE_ARRAY[BITE_FUNCTION_SELECTOR_SIZE_BYTES] = {0x42, 0x49, 0x54, 0x45};
#endif

static constexpr uint64_t BITE_EPOCH_ID_LEN = sizeof(uint64_t);
static constexpr uint64_t BITE_AES_KEY_LEN = libBLS::AES_256_KEY_SIZE_BYTES;
static constexpr uint64_t BITE_ENCRYPTED_AES_KEY_LEN = libBLS::CipheredKey::CIPHERED_KEY_SIZE_BYTES;
static constexpr uint64_t BITE_TE_PUBLIC_KEY_LEN = libBLS::G2_SIZE_BYTES;
static constexpr uint64_t BITE_TE_RANDOM_LEN = libBLS::RANDOM_SECRET_SIZE_BYTES;
static constexpr uint64_t BITE_CIPHERTEXT_MIN_LEN = BITE_ENCRYPTED_AES_KEY_LEN + BITE_TE_RANDOM_LEN + ADDRESS_SIZE;