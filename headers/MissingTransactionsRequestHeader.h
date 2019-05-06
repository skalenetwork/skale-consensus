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

    @file MissingTransactionsRequestHeader.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once



#include "Header.h"

class NodeInfo;
class BlockProposal;
class Schain;

class MissingTransactionsRequestHeader : public Header{


    uint64_t missingTransactionsCount;

public:


    MissingTransactionsRequestHeader();

    MissingTransactionsRequestHeader(ptr<map<uint64_t, ptr<partial_sha_hash>>> _missingMessages);

    void addFields(nlohmann::basic_json<> &j_) override;

    uint64_t getMissingTransactionsCount() const;

    void setMissingTransactionsCount(uint64_t _missingTransactionsCount);

};



