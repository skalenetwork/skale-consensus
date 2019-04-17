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

    @file BLSPrivateKey.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_BLSPRIVATEKEY_H
#define SKALED_BLSPRIVQATEKEY_H



class BLSPrivateKey{


private:

    size_t nodeCount;

    ptr<libff::alt_bn128_Fr> sk;



public:

    BLSPrivateKey(const string &k, node_count _nodeCount);

    ptr<BLSSigShare> sign(ptr<string> _msg, block_id _blockId, schain_index _signerIndex, node_id _signerNodeId);

    ptr<string> convertSigToString(const libff::alt_bn128_G1 &signature) const;
};


#endif


