#pragma once
#include <vector>


using EncryptedData = vector< std::uint8_t >;

class EncryptedAESKey;

class BiteCiphertext {

    ptr<EncryptedAESKey> encryptedAESKey;
    std::shared_ptr<EncryptedData> keyPlusEncryptedData;
    std::atomic_uint64_t epoch = 0;
    ptr<vector<uint8_t >> serializedData;

public:
    explicit BiteCiphertext( const shared_ptr< vector< std::uint8_t > >& data, epoch_id _currentEpochId);
    explicit BiteCiphertext( const shared_ptr< EncryptedData >& _encryptedKeyPlusData, uint64_t _epoch);

    [[nodiscard]] ptr<EncryptedAESKey> & getEncryptedAESKey();
    [[nodiscard]] const ptr< EncryptedData >& getKeyPlusEncryptedData() const;
    [[nodiscard]] uint64_t getEpoch();
    [[nodiscard]] ptr< vector< uint8_t > >& getSerializedData();

};
