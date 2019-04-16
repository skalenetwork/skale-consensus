//
// Created by stan on 18.04.18.
//

#include "../chains/Schain.h"

#include "../protocols/ProtocolKey.h"
#include "ConsensusProposalMessage.h"

ConsensusProposalMessage::ConsensusProposalMessage(Schain& _schain,  const block_id &_blockID, ptr<vector<bool>> _proposals) : Message(
        _schain.getSchainID(), MSG_CONSENSUS_PROPOSAL,
        msg_id(0), node_id(0), node_id(0), _blockID,
                schain_index(0)) {
    this->proposals = _proposals;

}


const ptr<vector<bool>> &ConsensusProposalMessage::getProposals() const {
    return proposals;
}
