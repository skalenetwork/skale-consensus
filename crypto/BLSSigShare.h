//
// Created by kladko on 7/4/19.
//

#ifndef SKALED_BLSSIGSHARE_H
#define SKALED_BLSSIGSHARE_H


namespace libff {
class alt_bn128_G1;
}

class BLSSigShare {

private:
    shared_ptr< libff::alt_bn128_G1 > sigShare;
    size_t signerIndex;

public:

    BLSSigShare( shared_ptr< string > _sigShare, size_t signerIndex );
    BLSSigShare( const shared_ptr< libff::alt_bn128_G1 >& sigShare, size_t signerIndex );

    shared_ptr< libff::alt_bn128_G1 > getSigShare() const;

    size_t getSignerIndex() const;

    shared_ptr< string > toString();
};


#endif  // SKALED_BLSSIGSHARE_H
