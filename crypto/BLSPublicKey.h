//
// Created by kladko on 7/4/19.
//

#ifndef SKALED_BLSPUBLICKEY_H
#define SKALED_BLSPUBLICKEY_H



class BLSPublicKey {
public:
    BLSPublicKey(const string &k1, const string &k2, const string &k3, const string &k4,
            size_t _totalSigners, size_t _requiredSigners);

protected:
    shared_ptr<libff::alt_bn128_G2> pk;
    size_t requiredSigners;
    size_t totalSigners;
};



#endif //SKALED_BLSPUBLICKEY_H
