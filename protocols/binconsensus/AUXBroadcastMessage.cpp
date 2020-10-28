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

#include "SkaleCommon.h"
#include "Log.h"
#include "crypto/ConsensusBLSSignature.h"
#include "crypto/BLAKE3Hash.h"

#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"

#include "chains/Schain.h"
#include "node/Node.h"

#include "messages/NetworkMessage.h"
#include "exceptions/InvalidArgumentException.h"
#include "crypto/ConsensusBLSSigShare.h"
#include "crypto/CryptoManager.h"

#include "protocols/ProtocolKey.h"


#include "BinConsensusInstance.h"

#include "AUXBroadcastMessage.h"


AUXBroadcastMessage::AUXBroadcastMessage(bin_consensus_round _round, bin_consensus_value _value, block_id _blockID,
                                         schain_index _proposerIndex, uint64_t _time,
                                         BinConsensusInstance &_sourceProtocolInstance)
        : NetworkMessage(MSG_AUX_BROADCAST, _blockID, _proposerIndex, _round, _value, _time,
                         _sourceProtocolInstance) {
    printPrefix = "a";
    auto schain = _sourceProtocolInstance.getSchain();
    CryptoPP::SHA256 sha256;
    auto bpi = getBlockProposerIndex();

    sha256.Update(reinterpret_cast < uint8_t * > ( &bpi), sizeof(bpi));
    sha256.Update(reinterpret_cast < uint8_t * > ( &this->r), sizeof(r));
    sha256.Update(reinterpret_cast < uint8_t * > ( &this->blockID), sizeof(blockID));
    sha256.Update(reinterpret_cast < uint8_t * > ( &this->schainID), sizeof(schainID));
    sha256.Update(reinterpret_cast < uint8_t * > ( &this->msgType), sizeof(msgType));

    auto buf = make_shared<array<uint8_t, SHA_HASH_LEN>>();
    sha256.Final(buf->data());
    auto hash = make_shared<BLAKE3Hash>(buf);
    this->sigShare = schain->getCryptoManager()->signBinaryConsensusSigShare(hash, _blockID);
    CHECK_STATE(sigShare);
    this->sigShareString = sigShare->toString();
}

AUXBroadcastMessage::AUXBroadcastMessage(node_id _srcNodeID, block_id _blockID, schain_index _blockProposerIndex,
                                         bin_consensus_round _r, bin_consensus_value _value, uint64_t _time, schain_id _schainId,
                                         msg_id _msgID, const string& _signature, schain_index _srcSchainIndex,
                                         const string& _ecdsaSig, const string& _pubKey, const string& _pkSig, Schain *_sChain)
        : NetworkMessage(
        MSG_AUX_BROADCAST, _srcNodeID, _blockID, _blockProposerIndex, _r, _value, _time, _schainId, _msgID,
        _signature, _ecdsaSig, _pubKey, _pkSig, _srcSchainIndex, _sChain->getCryptoManager()) {
    CHECK_ARGUMENT(!_signature.empty())
    printPrefix = "a";

};
