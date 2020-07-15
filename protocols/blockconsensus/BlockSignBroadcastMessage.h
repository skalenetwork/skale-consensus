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

    @file FinalizeMessage.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once

class BlockSignBroadcastMessage : public NetworkMessage{
public:

    BlockSignBroadcastMessage(block_id _blockID, schain_index _blockProposerIndex, uint64_t _time,
                              ProtocolInstance &_sourceProtocolInstance);

    BlockSignBroadcastMessage(node_id _srcNodeID, block_id _blockID,
                              schain_index _blockProposerIndex,uint64_t _time, schain_id _schainId,
                              msg_id _msgID, ptr<string> _sigShare,
                              schain_index _srcSchainIndex, ptr<string> _ecdsaSig,
                              ptr<string> _pubKey, ptr<string> _pkSig,
                              Schain *_sChain);

    virtual bin_consensus_round getRound() const override ;

    virtual bin_consensus_value getValue() const override ;
};

