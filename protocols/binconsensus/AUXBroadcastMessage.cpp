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

    @file AUXBroadcastMessage.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../../SkaleCommon.h"
#include "../../Log.h"
#include "../../thirdparty/json.hpp"
#include "../../exceptions/FatalError.h"
#include "../crypto/bls_include.h"
#include "../../crypto/ConsensusBLSSignature.h"
#include "../../crypto/SHAHash.h"

#include "../../chains/Schain.h"
#include "../../node/Node.h"

#include "../../messages/NetworkMessage.h"
#include "../../exceptions/InvalidArgumentException.h"
#include "../../crypto/ConsensusBLSSigShare.h"
#include "../../crypto/CryptoManager.h"

#include "../ProtocolKey.h"
#include "../ProtocolInstance.h"

#include "BinConsensusInstance.h"

#include "AUXBroadcastMessage.h"


AUXBroadcastMessage::AUXBroadcastMessage(bin_consensus_round round, bin_consensus_value value,
                                         node_id destinationNodeID, block_id _blockID, schain_index _blockProposer,
                                         BinConsensusInstance &sourceProtocolInstance)
        : NetworkMessage(AUX_BROADCAST, destinationNodeID, _blockID, _blockProposer, round, value,
                         sourceProtocolInstance) {
    printPrefix = "a";

    auto schain = sourceProtocolInstance.getSchain();


    CryptoPP::SHA256 sha3;


    auto bpi = getBlockProposerIndex();

    sha3.Update(reinterpret_cast < uint8_t * > ( &bpi), sizeof(bpi));
    sha3.Update(reinterpret_cast < uint8_t * > ( &this->r), sizeof(r));
    sha3.Update(reinterpret_cast < uint8_t * > ( &this->blockID), sizeof(blockID));
    sha3.Update(reinterpret_cast < uint8_t * > ( &this->schainID), sizeof(schainID));
    sha3.Update(reinterpret_cast < uint8_t * > ( &this->msgType), sizeof(msgType));

    auto buf = make_shared<array<uint8_t, SHA3_HASH_LEN>>();
    sha3.Final(buf->data());
    auto hash = make_shared<SHAHash>(buf);

    auto node = schain->getNode();

    if (node->isBlsEnabled()) {
        this->sigShare = schain->getCryptoSigner()->sign(hash, _blockID);
        this->sigShareString = sigShare->toString();
    } else {
        this->sigShareString = make_shared<string>("");
    }

}


AUXBroadcastMessage::AUXBroadcastMessage( node_id _srcNodeID, node_id _dstNodeID, block_id _blockID,
    schain_index _blockProposerIndex, bin_consensus_round _r, bin_consensus_value _value,
    schain_id _schainId, msg_id _msgID, uint32_t _ip, ptr< string > _signature,
    schain_index _srcSchainIndex, size_t _totalSigners, size_t _requiredSigners )
    : NetworkMessage(
        AUX_BROADCAST, _srcNodeID, _dstNodeID, _blockID, _blockProposerIndex, _r, _value, _schainId, _msgID, _ip,
        _signature, _srcSchainIndex, _totalSigners,_requiredSigners) {
    printPrefix = "a";

};
