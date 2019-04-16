//
// Created by kladko on 3/29/19.
//



#ifndef SKALED_BLSPUBLICKEY_H
#define SKALED_BLSPUBLICKEY_H



class BLSPublicKey{


private:

    size_t nodeCount;
    ptr<libff::alt_bn128_G2> pk;

public:

    BLSPublicKey(const string &k1, const string &k2, const string &k3, const string &k4, node_count _nodeCount);

};


#endif


