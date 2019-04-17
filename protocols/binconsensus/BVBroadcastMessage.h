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

    @file BVBroadcastMessage.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once





class BinConsensusInstance;


class BVBroadcastMessage : public NetworkMessage{
public:




    BVBroadcastMessage(node_id destinationNodeID, block_id _blockID,
                           schain_index _blockProposerIndex, bin_consensus_round r,
                           bin_consensus_value value, BinConsensusInstance &sourceProtocolInstance);

    BVBroadcastMessage(node_id _srcNodeID, node_id _dstNodeID,
                                           block_id _blockID, schain_index _blockProposerIndex,
                                           bin_consensus_round _r,
                                           bin_consensus_value _value,
                                           schain_id _schainId, msg_id _msgID, uint32_t _ip);


};

