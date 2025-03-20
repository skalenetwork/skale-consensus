#pragma once


using namespace std;

class AES256Key {
    array< uint8_t, HASH_LEN > aes256Key;

public:

    explicit AES256Key(){};


    void print();

    uint8_t at( uint32_t _position );

    int compare( AES256Key& _key2 );

    uint8_t* data() { return aes256Key.data(); };

    const array< uint8_t, AES256_KEY_LEN >& getKey() const;

    static AES256Key fromHex( const string& _hex );

    string toHex();

    static BLAKE3Hash calculateHash( const ptr< vector< uint8_t > >& _data );

};
