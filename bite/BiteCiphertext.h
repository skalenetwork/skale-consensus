#pragma once
#include <vector>
#include <cstdint>
#include <memory>
#include <optional>
#include <atomic>
#include <crypto/EncryptedAESKey.h>

using EncryptedData = std::vector< uint8_t >;

class BiteCiphertext {
    // optional to allow for lazy initialization - is always set in constructors
    std::optional<EncryptedAESKey> encryptedAESKey;

    std::shared_ptr<EncryptedData> keyPlusEncryptedData;
    std::atomic_uint64_t epoch = 0;
    std::shared_ptr<std::vector<uint8_t>> serializedData;

public:
    explicit BiteCiphertext( const std::shared_ptr< std::vector< std::uint8_t > >& data, epoch_id _currentEpochId);

    [[nodiscard]] EncryptedAESKey & getEncryptedAESKey();
    [[nodiscard]] const std::shared_ptr< EncryptedData >& getKeyPlusEncryptedData() const;
    [[nodiscard]] uint64_t getEpoch();
    [[nodiscard]] std::shared_ptr< std::vector< uint8_t > >& getSerializedData();

};
