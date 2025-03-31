#pragma once


using EncryptedData = vector< std::uint8_t >;

class EncryptedAESKey;

class BITEDataField {

    ptr<EncryptedAESKey> encryptedAESKey;
    std::shared_ptr<EncryptedData> encryptedData;
    uint64_t epoch = 0;
    ptr<vector<uint8_t >> serializedData;
    explicit BITEDataField( const shared_ptr< vector< std::uint8_t > >& data );

public:
    BITEDataField( const ptr<EncryptedAESKey>& _encryptedAESKey,
        const shared_ptr< EncryptedData >& _encryptedData, uint64_t _epoch );

    [[nodiscard]] ptr<EncryptedAESKey> & getEncryptedAESKey();
    [[nodiscard]] ptr< EncryptedData >& getEncryptedData();
    [[nodiscard]] uint64_t getEpoch();
    [[nodiscard]] ptr< vector< uint8_t > >& getSerializedData();

    [[nodiscard]] static ptr<BITEDataField> createIfMagicMatches(ptr<vector<uint8_t >>& _data);
};


