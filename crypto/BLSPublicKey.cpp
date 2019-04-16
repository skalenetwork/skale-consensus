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

    @file BLSPublicKey.cpp
    @author Stan Kladko
    @date 2019
*/


#include "../SkaleConfig.h"
#include "../thirdparty/json.hpp"
#include "../Log.h"
#include "../network/Utils.h"

#include "../exceptions/InvalidArgumentException.h"

#include "bls_include.h"

#include "BLSPublicKey.h"

BLSPublicKey::BLSPublicKey(const string &k1, const string &k2, const string &k3, const string &k4, node_count _nodeCount) :
       nodeCount(_nodeCount) {




    pk = make_shared<libff::alt_bn128_G2>();

    pk->X.c0 = libff::alt_bn128_Fq(k1.c_str());
    pk->X.c1 = libff::alt_bn128_Fq(k2.c_str());
    pk->Y.c0 = libff::alt_bn128_Fq(k3.c_str());
    pk->Y.c1 = libff::alt_bn128_Fq(k4.c_str());
    pk->Z.c0 = libff::alt_bn128_Fq::one();
    pk->Z.c1 = libff::alt_bn128_Fq::zero();

    if (pk->X.c0 == libff::alt_bn128_Fq::zero() ||
            pk->X.c1 == libff::alt_bn128_Fq::zero() ||
            pk->Y.c0 == libff::alt_bn128_Fq::zero() ||
            pk->Y.c1 == libff::alt_bn128_Fq::zero()) {

        BOOST_THROW_EXCEPTION(InvalidArgumentException("Public Key is equal to zero or corrupt", __CLASS_NAME__));
    }



}
