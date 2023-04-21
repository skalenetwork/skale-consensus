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


ptr< BLAKE3Hash > AUXBroadcastMessage::getCommonCoinHash() {
    HASH_INIT( hashObj );
    auto bpi = getBlockProposerIndex();

    HASH_UPDATE( hashObj, bpi );
    HASH_UPDATE( hashObj, this->r );
    HASH_UPDATE( hashObj, this->blockID );
    HASH_UPDATE( hashObj, this->schainID );
    HASH_UPDATE( hashObj, this->msgType );

    auto hash = make_shared< BLAKE3Hash >();
    HASH_FINAL( hashObj, hash->data() );
    return hash;
}


AUXBroadcastMessage::AUXBroadcastMessage( bin_consensus_round _round, bin_consensus_value _value,
    block_id _blockID, schain_index _proposerIndex, uint64_t _time,
    BinConsensusInstance& _sourceProtocolInstance )
    : NetworkMessage( MSG_AUX_BROADCAST, _blockID, _proposerIndex, _round, _value, _time,
          _sourceProtocolInstance ) {
    printPrefix = "a";
    auto schain = _sourceProtocolInstance.getSchain();
    HASH_INIT( hashObj );
    auto bpi = getBlockProposerIndex();

    HASH_UPDATE( hashObj, bpi );
    HASH_UPDATE( hashObj, this->r );
    HASH_UPDATE( hashObj, this->blockID );
    HASH_UPDATE( hashObj, this->schainID );
    HASH_UPDATE( hashObj, this->msgType );

    auto hash = getCommonCoinHash();
    CHECK_STATE( hash );

    if ( ( uint64_t ) _round >= COMMON_COIN_ROUND ) {
        this->sigShare = schain->getCryptoManager()->signBinaryConsensusSigShare(
            *hash, _blockID, ( uint64_t ) _round );
        this->sigShareString = sigShare->toString();
    } else {
        this->sigShare = nullptr;
        this->sigShareString = "";
    }
}

AUXBroadcastMessage::AUXBroadcastMessage( node_id _srcNodeID, block_id _blockID,
    schain_index _blockProposerIndex, bin_consensus_round _r, bin_consensus_value _value,
    uint64_t _time, schain_id _schainId, msg_id _msgID, const string& _blsSigShare,
    schain_index _srcSchainIndex, const string& _ecdsaSig, const string& _pubKey,
    const string& _pkSig, Schain* _sChain )
    : NetworkMessage( MSG_AUX_BROADCAST, _srcNodeID, _blockID, _blockProposerIndex, _r, _value,
          _time, _schainId, _msgID, _blsSigShare, _ecdsaSig, _pubKey, _pkSig, _srcSchainIndex,
          _sChain->getCryptoManager() ) {
    printPrefix = "a";

    if ( _r >= COMMON_COIN_ROUND && _sChain->getNode()->isSgxEnabled() ) {
        CHECK_STATE( !_blsSigShare.empty() )
    }
};
