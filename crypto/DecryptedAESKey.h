#pragma once

class DecryptedAESKey {
protected:
    block_id blockId = 0;

    uint64_t totalDecryptors = 0;
    uint64_t requiredDecryptors = 0;

    std::array< uint8_t, AES_KEY_LEN > aesKey;

public:
    DecryptedAESKey( const string& _key, const block_id _blockId,
        const transaction_index _transactionIndex, uint64_t _totalDecryptors,
        uint64_t _requiredDecryptors );

    [[nodiscard]] block_id getBlockId() const;

    string toHex();

    static BLAKE3Hash calculateHash( const ptr< vector< uint8_t > >& _data );

    void print();


    uint8_t at( uint32_t _position );

    int compare( DecryptedAESKey& _key2 );

    virtual ~DecryptedAESKey();
};
