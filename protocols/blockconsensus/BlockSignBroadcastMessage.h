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

#pragma once

#include "messages/NetworkMessage.h"

#ifdef BITE2
class BlockConsensusAgent;
#endif

class BlockSignBroadcastMessage : public NetworkMessage {
#ifdef BITE2
    friend class BlockConsensusAgent;
protected:
    string reencryptionSigShareString;
    ptr< ThresholdSigShare > reencryptionSigShare;
#endif

public:
    BlockSignBroadcastMessage( block_id _blockID,
#ifdef BITE
    epoch_id _epochID,
#endif
    schain_index _blockProposerIndex, uint64_t _time,
        ProtocolInstance& _sourceProtocolInstance );

    BlockSignBroadcastMessage( node_id _srcNodeID, block_id _blockID,
#ifdef BITE
    epoch_id _epochID,
#endif
        schain_index _blockProposerIndex, uint64_t _time, schain_id _schainId, msg_id _msgID,
        const string& _sigShare, schain_index _srcSchainIndex, const string& _ecdsaSig,
        const string& _pubKey, const string& _pkSig, Schain* _sChain
#ifdef BITE2
        , const string& _reencryptionSigShare = ""
#endif
        );

    virtual bin_consensus_round getRound() const override;

    virtual bin_consensus_value getValue() const override;

#ifdef BITE2
    ptr< ThresholdSigShare > getReencryptionSigShare() const;

protected:
    void serializeToStringChild( rapidjson::Writer< rapidjson::StringBuffer >& _writer ) override;
    void updateWithChildHash( blake3_hasher& _hasher ) override;
#endif
};
