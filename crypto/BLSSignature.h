//
// Created by kladko on 7/4/19.
//

#ifndef SKALED_BLSSIGNATURE_H
#define SKALED_BLSSIGNATURE_H

#include "../SkaleCommon.h"
class BLSSignature {
protected:
    shared_ptr<libff::alt_bn128_G1> sig;
public:

    BLSSignature(shared_ptr<string> s);
    BLSSignature( const shared_ptr< libff::alt_bn128_G1 >& sig );
    shared_ptr<libff::alt_bn128_G1> getSig() const;
    ptr< basic_string< char > > toString();
};



#endif //SKALED_BLSSIGNATURE_H
