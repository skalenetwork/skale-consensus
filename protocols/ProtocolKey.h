/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file ProtocolKey.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once

class NetworkMessage;


class ProtocolKey {

    const block_id blockID = 0;

    const schain_index  blockProposerIndex = 0;

public:

    [[nodiscard ]] block_id getBlockID() const;

    [[nodiscard ]]  schain_index getBlockProposerIndex() const;

    ProtocolKey(block_id _blockId, schain_index _blockProposerIndex);

    ProtocolKey(const ProtocolKey& key);

    virtual ~ProtocolKey()  = default;

};

bool operator<(const ProtocolKey &l, const ProtocolKey &r);


