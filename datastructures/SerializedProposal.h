/*
    Copyright (C) 2021 SKALE Labs

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

    @file SerializedProposal.h
    @author Stan Kladko
    @date 2023 -
*/


#ifndef SERIALIZED_PROPOSAL_H
#define SERIALIZED_PROPOSAL_H




class SerializedProposal {
public:
    explicit SerializedProposal( const ptr< vector< uint8_t > > bytes );
    const ptr< vector< uint8_t > > getBytes() const;

private:
    ptr<vector<uint8_t>> bytes;
public:

};


#endif
