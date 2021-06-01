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
#include "crypto/BLAKE3Hash.h"

#include "protocols/ProtocolKey.h"
#include "protocols/ProtocolInstance.h"
#include "crypto/CryptoManager.h"
#include "chains/Schain.h"

#include "BlockSignBroadcastMessage.h"
#include "protocols/binconsensus/BinConsensusInstance.h"
#include "crypto/ThresholdSigShare.h"


bin_consensus_round BlockSignBroadcastMessage::getRound() const {
    CHECK_STATE(false);
}

bin_consensus_value BlockSignBroadcastMessage::getValue() const {
    CHECK_STATE(false);
}

BLAKE3Hash getBlockHash(uint64_t _blockProposerIndex, uint64_t _blockId, uint64_t _schainId) {
    uint32_t msgType = MSG_BLOCK_SIGN_BROADCAST;
    BLAKE3Hash _hash;
    HASH_INIT(hashObj)
    HASH_UPDATE(hashObj, _blockProposerIndex)
    HASH_UPDATE(hashObj, _blockId)
    HASH_UPDATE(hashObj, _schainId)
    HASH_UPDATE(hashObj, msgType);
    HASH_FINAL(hashObj, _hash.data());
    return _hash;
}


BlockSignBroadcastMessage::BlockSignBroadcastMessage(block_id _blockID, schain_index _blockProposerIndex,
                                                     uint64_t _time,
                                                     ProtocolInstance &_sourceProtocolInstance)
        : NetworkMessage(MSG_BLOCK_SIGN_BROADCAST, _blockID, _blockProposerIndex, 4, 0, _time,
                         _sourceProtocolInstance) {
    printPrefix = "f";

    auto schain = _sourceProtocolInstance.getSchain();
    auto hash = getBlockHash(
        (uint64_t ) getBlockProposerIndex(),
        (uint64_t) _blockID,
        (uint64_t) schain->getSchainID());

    this->sigShare = schain->getCryptoManager()->signBlockSigShare(hash, _blockID);
    this->sigShareString = sigShare->toString();
}


BlockSignBroadcastMessage::BlockSignBroadcastMessage(node_id _srcNodeID, block_id _blockID,
                                                     schain_index _blockProposerIndex,uint64_t _time, schain_id _schainId,
                                                     msg_id _msgID, const string& _sigShare,
                                                     schain_index _srcSchainIndex, const string& _ecdsaSig,
                                                     const string& _pubKey, const string& _pkSig,
                                                     Schain *_sChain)
    : NetworkMessage(
        MSG_BLOCK_SIGN_BROADCAST, _srcNodeID, _blockID, _blockProposerIndex, 4, 0, _time, _schainId, _msgID, _sigShare,
        _ecdsaSig, _pubKey, _pkSig,
        _srcSchainIndex, _sChain->getCryptoManager()) {
    CHECK_ARGUMENT(!_sigShare.empty());
    printPrefix = "F";
};

