#pragma once


using EncryptedData = vector< std::uint8_t >;

class EncryptedAESKey;

class BiteDataField {

    ptr<EncryptedAESKey> encryptedAESKey;
    std::shared_ptr<EncryptedData> keyPlusEncryptedData;
    uint64_t epoch = 0;
    ptr<vector<uint8_t >> serializedData;
    explicit BiteDataField( const shared_ptr< vector< std::uint8_t > >& data, bool _useRealCrypto = true );

public:
    BiteDataField( const shared_ptr< EncryptedData >& _encryptedKeyPlusData, uint64_t _epoch, bool _useRealCrypto = true );

    [[nodiscard]] ptr<EncryptedAESKey> & getEncryptedAESKey();
    [[nodiscard]] const ptr< EncryptedData >& getKeyPlusEncryptedData() const;
    [[nodiscard]] uint64_t getEpoch();
    [[nodiscard]] ptr< vector< uint8_t > >& getSerializedData();

    [[nodiscard]] static ptr<BiteDataField> createIfMagicMatches(ptr<vector<uint8_t >>& _data, ptr<vector<uint8_t>>& _to, bool _useRealCrypto = true);
};