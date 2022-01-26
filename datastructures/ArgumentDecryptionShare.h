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

    @file BlockDecryptionShare.h
    @author Stan Kladko
    @date 2019
*/
#ifndef SKALED_BLOCKPROPOSALFRAGMENT_H
#define SKALED_BLOCKPROPOSALFRAGMENT_H

class ArgumentDecryptionShare {

    string data; // tsafe

    const block_id blockId  = 0;

    const uint64_t totalShares  = 0;
    const te_share_index schainIndex  = 0;

public:

    ArgumentDecryptionShare(const block_id & _blockId, uint64_t _totalShares, const te_share_index &_schainIndex,
                            const string & _data);

    [[nodiscard]] block_id getBlockId() const;

    [[nodiscard]] uint64_t getTotalShares() const;

    [[nodiscard]] te_share_index getSchainIndex() const;

};


#endif //SKALED_BLOCKPROPOSALFRAGMENT_H
