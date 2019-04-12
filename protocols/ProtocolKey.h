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

    @file ProtocolKey.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once

class NetworkMessage;


class ProtocolKey  {


public:

    block_id getBlockID() const;

    schain_index getBlockProposerIndex() const;

private:

    const block_id blockID;

    const schain_index  blockProposerIndex;

public:

    ProtocolKey(block_id _blockId, schain_index _blockProposerIndex);


    ProtocolKey(const ProtocolKey& key);

    virtual ~ProtocolKey() {
    }

};

bool operator<(const ProtocolKey &l, const ProtocolKey &r);


