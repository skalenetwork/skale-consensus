//
// Created by kladko on 7/4/19.
//

#ifndef SKALED_BLSSIGSHARE_H
#define SKALED_BLSSIGSHARE_H

#include <cstdint>
namespace libff {
class alt_bn128_G1;
}

class BLSSigShare {

private:
    ptr< libff::alt_bn128_G1 > sigShare;
    size_t signerIndex;

public:

    BLSSigShare( ptr< string > _sigShare, size_t signerIndex );
    BLSSigShare( const ptr< libff::alt_bn128_G1 >& sigShare, size_t signerIndex );

    ptr< libff::alt_bn128_G1 > getSigShare() const;

    size_t getSignerIndex() const;

    ptr< string > toString();
};


#endif  // SKALED_BLSSIGSHARE_H
