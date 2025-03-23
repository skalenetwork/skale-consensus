#pragma once


using EncryptedData = vector< std::uint8_t >;
using EncryptedAESKey = array< uint8_t, ENCRYPTED_AES_KEY_LEN >;

class BITEDataField {
    EncryptedAESKey  encryptedAESKey{};
    std::shared_ptr<EncryptedData> encryptedData;
    uint64_t epoch = 0;

    explicit BITEDataField( const shared_ptr< vector< std::uint8_t > >& data );
public:
    [[nodiscard]] const EncryptedAESKey & getEncryptedAESKey() const;
    [[nodiscard]] const shared_ptr< EncryptedData >& getEncryptedData() const;
    [[nodiscard]] uint64_t getEpoch() const;
    [[nodiscard]] static ptr<BITEDataField> createIfMagicMatches(ptr<vector<uint8_t >>& _data);
};


