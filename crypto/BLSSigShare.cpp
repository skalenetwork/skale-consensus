//
// Created by kladko on 3/29/19.
//


#include "../SkaleConfig.h"
#include "../thirdparty/json.hpp"
#include "../Log.h"
#include "../network/Utils.h"
#include "../exceptions/InvalidArgumentException.h"

#include "bls_include.h"

#include "BLSSigShare.h"


ptr<string> BLSSigShare::toString() {
    char str[512];


    gmp_sprintf(str, "%Nd:%Nd", sig->X.as_bigint().data,
                libff::alt_bn128_Fq::num_limbs, sig->Y.as_bigint().data, libff::alt_bn128_Fq::num_limbs);

    return make_shared<string>(str);

}

BLSSigShare::BLSSigShare(ptr<string> _s, block_id _blockID, schain_index _signerIndex, node_id _nodeId) :
    blockId(_blockID), signerIndex(_signerIndex), signerNodeId(_nodeId) {

    if (_s->size() > BLS_SIG_LEN) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Signature too long", __CLASS_NAME__));
    }

    auto position = _s->find(":");

    if (position == string::npos) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Misformatted sig:" + *_s, __CLASS_NAME__));
    }

    if (position >= BLS_COMPONENT_LEN || _s->size() - position > BLS_COMPONENT_LEN) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Misformatted sig:" + *_s, __CLASS_NAME__));
    }


    auto component1 = _s->substr(0, position);
    auto component2 = _s->substr(position + 1);


    for (char &c : component1) {
        if (!(c >= '0' && c <= '9')) {
            BOOST_THROW_EXCEPTION(InvalidArgumentException("Misformatted char:" + to_string((int)c) + " in component 1:"
                                                           + component1, __CLASS_NAME__));
        }
    }


    for (char &c : component2) {
        if (!(c >= '0' && c <= '9')) {
            BOOST_THROW_EXCEPTION(
                    InvalidArgumentException("Misformatted char:" + to_string((int)c) + " in component 2:" + component2,
                                             __CLASS_NAME__));
        }
    }


    libff::bigint<4> X(component1.c_str());
    libff::bigint<4> Y(component2.c_str());
    libff::bigint<4> Z("1");

    sig = make_shared<libff::alt_bn128_G1>(X, Y, Z);

}

const ptr<libff::alt_bn128_G1> &BLSSigShare::getSig() const {
    return sig;
}

const block_id &BLSSigShare::getBlockId() const {
    return blockId;
}

const schain_index &BLSSigShare::getSignerIndex() const {
    return signerIndex;
}

const node_id &BLSSigShare::getSignerNodeId() const {
    return signerNodeId;
}

BLSSigShare::BLSSigShare(ptr<libff::alt_bn128_G1> &_s, block_id _blockID, schain_index _signerIndex, node_id _nodeID) :
    sig(_s), blockId(_blockID), signerIndex(_signerIndex), signerNodeId(_nodeID){}
