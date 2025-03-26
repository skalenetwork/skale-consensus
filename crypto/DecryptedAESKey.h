#pragma once

class EncryptedAESKey;

class DecryptedAESKey {

    std::array< uint8_t, BITE_AES_KEY_LEN > aesKey;

public:
    string toHex();

    void print();

    explicit DecryptedAESKey(const std::array<uint8_t, BITE_AES_KEY_LEN> &aes_key)
        : aesKey(aes_key) {
    }

    uint8_t at( uint32_t _position );

    int compare( DecryptedAESKey& _key2 );


    static std::shared_ptr<EncryptedAESKey> generate();




    ptr<vector<uint8_t>> encryptData(ptr<vector<uint8_t>>& data);

    ptr<vector<uint8_t>> decryptData(ptr<vector<uint8_t>>& data);


    virtual ~DecryptedAESKey();
};
