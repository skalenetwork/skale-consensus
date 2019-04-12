/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file Transaction.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once

#include "../datastructures/DataStructure.h"

class SHAHash;



class Transaction : public DataStructure {


private:

    ptr<vector<uint8_t >> data = nullptr;

    ptr<SHAHash> hash = nullptr;

    ptr<partial_sha_hash> partialHash = nullptr;

protected:

    Transaction(const ptr<vector<uint8_t>> data);

public:



    const ptr<vector<uint8_t>>& getData() const;


    ptr<SHAHash> getHash();

    ptr<partial_sha_hash> getPartialHash();

    virtual ~Transaction();


};



