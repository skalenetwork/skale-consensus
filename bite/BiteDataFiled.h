#pragma once


using EncryptedData = vector< std::uint8_t >;

class EncryptedAESKey;

class BiteDataField {

    ptr<EncryptedAESKey> encryptedAESKey;
    std::shared_ptr<EncryptedData> encryptedData;
    std::shared_ptr<EncryptedData> keyPlusEncryptedData;
    uint64_t epoch = 0;
    ptr<vector<uint8_t >> serializedData;
    explicit BiteDataField( const shared_ptr< vector< std::uint8_t > >& data );

public:
    BiteDataField( const shared_ptr< EncryptedData >& _encryptedKeyPlusData, uint64_t _epoch);

    [[nodiscard]] ptr<EncryptedAESKey> & getEncryptedAESKey();
    [[nodiscard]] ptr< EncryptedData >& getEncryptedData();
    [[nodiscard]] uint64_t getEpoch();
    [[nodiscard]] ptr< vector< uint8_t > >& getSerializedData();

    [[nodiscard]] static ptr<BiteDataField> createIfMagicMatches(ptr<vector<uint8_t >>& _data);
    const shared_ptr< EncryptedData >& getKeyPlusEncryptedData() const;
};


