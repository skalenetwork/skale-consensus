/*
    Copyright (C) 2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file BLSSigShare.cpp
    @author Stan Kladko
    @date 2019
*/


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
