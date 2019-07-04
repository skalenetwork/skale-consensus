//
// Created by kladko on 7/4/19.
//

#include "../Log.h"
#include "../SkaleCommon.h"
#include "../crypto/bls_include.h"
#include "../network/Utils.h"
#include "../thirdparty/json.hpp"
#include "BLSSignature.h"

ptr<libff::alt_bn128_G1> BLSSignature::getSig() const {
    return sig;
}
BLSSignature::BLSSignature(const shared_ptr<libff::alt_bn128_G1> & sig):sig(sig){}

BLSSignature::BLSSignature( shared_ptr< string > _s ) {

    if (_s->size() > BLS_MAX_SIG_LEN) {
        BOOST_THROW_EXCEPTION(runtime_error("Signature too long"));
    }

    auto position = _s->find(":");

    if (position == string::npos) {
        BOOST_THROW_EXCEPTION(runtime_error("Misformatted sig:" + *_s));
    }

    if (position >= BLS_MAX_COMPONENT_LEN || _s->size() - position > BLS_MAX_COMPONENT_LEN) {
        BOOST_THROW_EXCEPTION(runtime_error("Misformatted sig:" + *_s));
    }


    auto component1 = _s->substr(0, position);
    auto component2 = _s->substr(position + 1);


    for (char &c : component1) {
        if (!(c >= '0' && c <= '9')) {
            BOOST_THROW_EXCEPTION(runtime_error("Misformatted char:" + to_string((int)c) + " in component 1:"
                                                + component1));
        }
    }


    for (char &c : component2) {
        if (!(c >= '0' && c <= '9')) {
            BOOST_THROW_EXCEPTION(
                runtime_error("Misformatted char:" + to_string((int)c) + " in component 2:" + component2));
        }
    }


    libff::bigint<4> X(component1.c_str());
    libff::bigint<4> Y(component2.c_str());
    libff::bigint<4> Z("1");

    sig = make_shared<libff::alt_bn128_G1>(X, Y, Z);


}