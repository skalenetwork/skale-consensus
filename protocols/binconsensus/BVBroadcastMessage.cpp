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

    @file BVBroadcastMessage.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../../SkaleConfig.h"
#include "../../Log.h"
#include "../../exceptions/FatalError.h"

#include "../../messages/NetworkMessage.h"

#include "../ProtocolKey.h"
#include "../ProtocolInstance.h"
#include "BinConsensusInstance.h"

#include "BVBroadcastMessage.h"


BVBroadcastMessage::BVBroadcastMessage(node_id destinationNodeID, block_id _blockID,
                                       schain_index _blockProposerIndex, bin_consensus_round r,
                                       bin_consensus_value value, BinConsensusInstance &sourceProtocolInstance)
        : NetworkMessage(BVB_BROADCAST, destinationNodeID, _blockID, _blockProposerIndex, r, value,
                         sourceProtocolInstance) {
    printPrefix = "b";
}


BVBroadcastMessage::BVBroadcastMessage(node_id _srcNodeID, node_id _dstNodeID, block_id _blockID,
                                       schain_index _blockProposerIndex,
                                       bin_consensus_round _r, bin_consensus_value _value, schain_id _schainId,
                                       msg_id _msgID,
                                       uint32_t _ip, ptr<string> _sigShare, schain_index _srcSchainIndex) : NetworkMessage(
        BVB_BROADCAST, _srcNodeID, _dstNodeID, _blockID, _blockProposerIndex, _r, _value, _schainId, _msgID, _ip, _sigShare,
        _srcSchainIndex) {
    printPrefix = "b";
};
