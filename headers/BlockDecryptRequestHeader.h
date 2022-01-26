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

    @file BlockDecryptRequestHeader.h
    @author Stan Kladko
    @date 2022-
*/

#pragma  once

#include "AbstractBlockRequestHeader.h"

class BlockDecryptRequestHeader : public AbstractBlockRequestHeader{


   te_share_index shareIndex;
   node_id        nodeID;
   ptr<map<uint64_t, string>> encryptedKeys;

public:

    BlockDecryptRequestHeader(Schain &_sChain, block_id _blockID,
            schain_index _proposerIndex, node_id _nodeID,
                               te_share_index _shareIndex,
                               ptr<map<uint64_t, string>> _encryptedKeys);

    void addFields(nlohmann::basic_json<> &jsonRequest) override;

    const node_id &getNodeId() const;

};



