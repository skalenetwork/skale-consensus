#pragma once
#include <vector>
#include <cstdint>
#include <memory>
#include <optional>
#include <atomic>
#include <SkaleCommon.h>
#include <crypto/EncryptedAESKey.h>

using EncryptedData = std::vector< uint8_t >;

class BiteCiphertext {
    // optional to allow for lazy initialization - is always set in constructors
    std::optional<EncryptedAESKey> encryptedAESKey;

    std::shared_ptr<EncryptedData> keyPlusEncryptedData;
    std::atomic_uint64_t epoch = 0;
    std::shared_ptr<std::vector<uint8_t>> serializedData;

    // True when the ciphertext is encrypted for another epoch than the one it was parsed for.
    // Only ever true for a CTX argument. With the current DKG a rotation changes the common
    // public key, so a CTX created in the last block of rotation R is still in skaled's queue
    // in the first block of rotation R+1, with arguments encrypted for the previous key. Such
    // a CTX stays in the block and is not decrypted.
    // Keep this after moving to DKGv3 that preserves the key across rotations: blocks
    // committed before that can contain such a CTX and are parsed again during catchup.
    // A regular encrypted transaction never has it set: the constructor throws for it instead.
    bool staleEpoch = false;

public:
    // _isCTXArgument: the ciphertext is an encrypted argument of a CTX. Only then a ciphertext
    // encrypted for another epoch is accepted and marked as stale; any other ciphertext throws.
    explicit BiteCiphertext( const std::shared_ptr< std::vector< std::uint8_t > >& data, epoch_id _currentEpochId,
                             bool _isCTXArgument = false );

    [[nodiscard]] EncryptedAESKey & getEncryptedAESKey();
    [[nodiscard]] const std::shared_ptr< EncryptedData >& getKeyPlusEncryptedData() const;
    [[nodiscard]] uint64_t getEpoch();
    [[nodiscard]] bool isStaleEpoch() const;
    [[nodiscard]] std::shared_ptr< std::vector< uint8_t > >& getSerializedData();

};
