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

    @file BLSSignature.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_BLSSIGNATURE_H
#define SKALED_BLSSIGNATURE_H



class BLSSignature {


    ptr<libff::alt_bn128_G1> sig;
    block_id blockId;

public:

    BLSSignature(ptr<string> s);


    ptr<string> toString();

    BLSSignature(ptr<string> _s, block_id _blockID);

    BLSSignature(ptr<libff::alt_bn128_G1>& _s, block_id _blockID);

    const block_id &getBlockId() const;


    const ptr<libff::alt_bn128_G1>& getSig() const;

};


#endif //SKALED_BLSSIGNATURE_H


