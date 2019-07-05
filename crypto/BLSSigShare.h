/*
    Copyright (C) 2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file BLSSigShare.h
    @author Stan Kladko
    @date 2019
*/
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
