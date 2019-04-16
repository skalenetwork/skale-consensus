#pragma  once

#include "../datastructures/DataStructure.h"

class SHAHash;



class Transaction : public DataStructure {


private:

    ptr<vector<uint8_t >> data = nullptr;

    ptr<SHAHash> hash = nullptr;

    ptr<partial_sha_hash> partialHash = nullptr;

protected:

    Transaction(const ptr<vector<uint8_t>> data);

public:



    const ptr<vector<uint8_t>>& getData() const;


    ptr<SHAHash> getHash();

    ptr<partial_sha_hash> getPartialHash();

    virtual ~Transaction();


};



