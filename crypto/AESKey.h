#pragma once


using namespace std;

class AESKey {
    array< uint8_t, AES_KEY_LEN > aesKey;

public:

    explicit AESKey(){};


    void print();

    uint8_t at( uint32_t _position );

    int compare( AESKey& _key2 );

    uint8_t* data() { return aesKey.data(); };

    const array< uint8_t, AES_KEY_LEN >& getKey() const;

    static AESKey fromHex( const string& _hex );

    string toHex();

    static BLAKE3Hash calculateHash( const ptr< vector< uint8_t > >& _data );

};
