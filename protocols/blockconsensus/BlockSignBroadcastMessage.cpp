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

#ifdef BITE2
#include "thirdparty/rapidjson/writer.h"
#include "thirdparty/rapidjson/stringbuffer.h"
#include "protocols/blockconsensus/ConsensusSignatureDomains.h"
#endif


bin_consensus_round BlockSignBroadcastMessage::getRound() const {
    CHECK_STATE( false );
}

bin_consensus_value BlockSignBroadcastMessage::getValue() const {
    CHECK_STATE( false );
}


BlockSignBroadcastMessage::BlockSignBroadcastMessage( block_id _blockID,
#ifdef BITE
    epoch_id _epochID,
#endif
    schain_index _blockProposerIndex, uint64_t _time, ProtocolInstance& _sourceProtocolInstance
#ifdef BITE2
    , bool _includeOffchainSigShare
#endif
)
    : NetworkMessage( MSG_BLOCK_SIGN_BROADCAST, _blockID,
#ifdef BITE
    _epochID,
#endif
    _blockProposerIndex, 4, 0, _time,
          _sourceProtocolInstance ) {
    printPrefix = "f";

    auto schain = _sourceProtocolInstance.getSchain();
    auto hash = BLAKE3Hash::getConsensusHash( ( uint64_t ) getBlockProposerIndex(),
        ( uint64_t ) _blockID, ( uint64_t ) schain->getSchainID() );

    this->sigShare = schain->getCryptoManager()->signBlockSigShare( hash, _blockID );
    this->sigShareString = sigShare->toString();

#ifdef BITE2
    if ( _includeOffchainSigShare ) {
        // compute additional offchain signature using domain separation.
        // This signature will be used to derive a random value seen only by validators.
        auto& signatureDomain = blockconsensus::REENCRYPTION_RANDOM_DOMAIN;

        // Compute offchain block sig share as hash( blockHash || signatureDomain )
        auto data = make_shared< vector< uint8_t > >();
        const auto& baseHash = hash.getHash();
        data->reserve( baseHash.size() + signatureDomain.size() );
        data->insert( data->end(), baseHash.begin(), baseHash.end() );
        data->insert( data->end(), signatureDomain.begin(), signatureDomain.end() );
        auto offchainHash = BLAKE3Hash::calculateHash( data );

        // Sign offchain digest and store in message
        auto offchainSigShare = schain->getCryptoManager()->signBlockSigShare( offchainHash, _blockID );
        this->offchainSigShareString = offchainSigShare->toString();
        this->offchainSigShare = offchainSigShare;
    }
#endif
}


BlockSignBroadcastMessage::BlockSignBroadcastMessage( node_id _srcNodeID, block_id _blockID,
#ifdef BITE
    epoch_id _epochID,
#endif
    schain_index _blockProposerIndex, uint64_t _time, schain_id _schainId, msg_id _msgID,
    const string& _sigShare, schain_index _srcSchainIndex, const string& _ecdsaSig,
    const string& _pubKey, const string& _pkSig, Schain* _sChain
#ifdef BITE2
    , const string& _offchainSigShare
#endif
    )
    : NetworkMessage( MSG_BLOCK_SIGN_BROADCAST, _srcNodeID, _blockID,
#ifdef BITE
          _epochID,
#endif
    _blockProposerIndex, 4, 0,
          _time, _schainId, _msgID, _sigShare, _ecdsaSig, _pubKey, _pkSig, _srcSchainIndex,
          _sChain->getCryptoManager() ) {
    CHECK_ARGUMENT( !_sigShare.empty() );
    printPrefix = "F";

#ifdef BITE2
    this->offchainSigShareString = _offchainSigShare;
    if ( !_offchainSigShare.empty() ) {
        offchainSigShare = _sChain->getCryptoManager()->createSigShare(
            _offchainSigShare, _schainId, _blockID, _srcSchainIndex, false );
        CHECK_STATE( offchainSigShare );
    }
#endif
};

#ifdef BITE2
ptr< ThresholdSigShare > BlockSignBroadcastMessage::getOffchainSigShare() const {
    return offchainSigShare;
}

void BlockSignBroadcastMessage::serializeToStringChild(
    rapidjson::Writer< rapidjson::StringBuffer >& _writer ) {
    if ( !offchainSigShareString.empty() ) {
        _writer.String( "ofss" );
        _writer.String( offchainSigShareString.data(), offchainSigShareString.size() );
    }
}

void BlockSignBroadcastMessage::updateWithChildHash( blake3_hasher& _hasher ) {
    uint32_t offchainSigShareLen = offchainSigShareString.size();
    HASH_UPDATE( _hasher, offchainSigShareLen );
    if ( offchainSigShareLen > 0 ) {
        blake3_hasher_update(
            &_hasher, ( unsigned char* ) offchainSigShareString.data(), offchainSigShareLen );
    }
}
#endif
