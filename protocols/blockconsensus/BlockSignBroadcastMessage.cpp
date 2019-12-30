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

    @file FinalizeMessage.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"

#include "messages/NetworkMessage.h"
#include "crypto/SHAHash.h"

#include "protocols/ProtocolKey.h"
#include "protocols/ProtocolInstance.h"
#include "crypto/CryptoManager.h"
#include "chains/Schain.h"

#include "BlockSignBroadcastMessage.h"
#include "protocols/binconsensus/BinConsensusInstance.h"
#include "crypto/ThresholdSigShare.h"


bin_consensus_round BlockSignBroadcastMessage::getRound() const {
    assert(false);
}

bin_consensus_value BlockSignBroadcastMessage::getValue() const {
    assert(false);
}


BlockSignBroadcastMessage::BlockSignBroadcastMessage(block_id _blockID, schain_index _blockProposerIndex,
                                                     ProtocolInstance &_sourceProtocolInstance)
        : NetworkMessage(MSG_BLOCK_SIGN_BROADCAST, _blockID, _blockProposerIndex, 0, 0,
                         _sourceProtocolInstance) {
    printPrefix = "f";

    auto schain = _sourceProtocolInstance.getSchain();
    CryptoPP::SHA256 sha256;
    auto bpi = getBlockProposerIndex();
    sha256.Update(reinterpret_cast < uint8_t * > ( &bpi), sizeof(bpi));
    sha256.Update(reinterpret_cast < uint8_t * > ( &this->blockID), sizeof(blockID));
    sha256.Update(reinterpret_cast < uint8_t * > ( &this->schainID), sizeof(schainID));
    sha256.Update(reinterpret_cast < uint8_t * > ( &this->msgType), sizeof(msgType));
    auto buf = make_shared<array<uint8_t, SHA_HASH_LEN>>();
    sha256.Final(buf->data());
    auto hash = make_shared<SHAHash>(buf);

    this->sigShare = schain->getCryptoManager()->signBlockSigShare(hash, _blockID);
    this->sigShareString = sigShare->toString();
}


BlockSignBroadcastMessage::BlockSignBroadcastMessage(node_id _srcNodeID, block_id _blockID,
                                                     schain_index _blockProposerIndex,
                                                     schain_id _schainId, msg_id _msgID, ptr<string> _sigShare,
                                                     schain_index _srcSchainIndex,
                                                     Schain *_sChain)
    : NetworkMessage(
        MSG_BLOCK_SIGN_BROADCAST, _srcNodeID, _blockID, _blockProposerIndex, 0, 0, _schainId, _msgID, _sigShare,
        _srcSchainIndex, _sChain->getCryptoManager(), _sChain->getTotalSigners(),
        _sChain->getRequiredSigners()) {
    CHECK_ARGUMENT(_sigShare);
    printPrefix = "F";
};

