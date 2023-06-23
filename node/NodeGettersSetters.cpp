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

    @file NodeGettersSetters.cpp
    @author Stan Kladko
    @date 2018
*/

#include "leveldb/db.h"

#include "SkaleCommon.h"
#include "Log.h"


#include "exceptions/InvalidArgumentException.h"
#include "exceptions/ParsingException.h"
#include "thirdparty/json.hpp"

#include "chains/TestConfig.h"

#include "crypto/bls_include.h"


#include "libBLS/bls/BLSPublicKey.h"
#include "libBLS/bls/BLSSignature.h"

#include "blockproposal/server/BlockProposalServerAgent.h"
#include "messages/NetworkMessageEnvelope.h"

#include "chains/Schain.h"


#include "network/Sockets.h"
#include "network/TCPServerSocket.h"
#include "network/ZMQNetwork.h"
#include "network/ZMQSockets.h"
#include "node/NodeInfo.h"

#include "catchup/server/CatchupServerAgent.h"
#include "exceptions/FatalError.h"
#include "messages/Message.h"

#include "db/BlockDB.h"
#include "db/BlockProposalDB.h"
#include "db/BlockSigShareDB.h"
#include "db/DAProofDB.h"
#include "db/DASigShareDB.h"
#include "db/InternalInfoDB.h"
#include "db/MsgDB.h"
#include "db/ProposalHashDB.h"
#include "db/ProposalVectorDB.h"

#include "db/ConsensusStateDB.h"
#include "db/PriceDB.h"
#include "db/RandomDB.h"
#include "db/SigDB.h"


#include "ConsensusEngine.h"
#include "ConsensusInterface.h"
#include "Node.h"

using namespace std;


uint64_t Node::getParamUint64( const string& _paramName, uint64_t paramDefault ) {
    auto result = std::getenv( _paramName.c_str() );

    if ( result != nullptr ) {
        errno = 0;
        auto value = strtoll( result, nullptr, 10 );
        if ( errno == 0 ) {
            return value;
        } else {
            BOOST_THROW_EXCEPTION( InvalidStateException(
                "Invalid value of env var " + _paramName + "=" + result, __CLASS_NAME__ ) );
        }
    }

    try {
        if ( cfg.find( _paramName ) != cfg.end() ) {
            return cfg.at( _paramName ).get< uint64_t >();
        } else {
            return paramDefault;
        }
    } catch ( ... ) {
        throw_with_nested(
            ParsingException( "Could not parse param " + _paramName, __CLASS_NAME__ ) );
    }
}

int64_t Node::getParamInt64( const string& _paramName, uint64_t _paramDefault ) {
    try {
        if ( cfg.find( _paramName ) != cfg.end() ) {
            return cfg.at( _paramName ).get< uint64_t >();
        } else {
            return _paramDefault;
        }

    } catch ( ... ) {
        throw_with_nested(
            ParsingException( "Could not parse param " + _paramName, __CLASS_NAME__ ) );
    }
}


string Node::getParamString( const string& _paramName, string& _paramDefault ) {
    auto result = std::getenv( _paramName.c_str() );

    if ( result != nullptr ) {
        return string( result );
    }


    try {
        if ( cfg.find( _paramName ) != cfg.end() ) {
            return string( cfg.at( _paramName ).get< string >() );
        } else {
            return string( _paramDefault );
        }

    } catch ( ... ) {
        throw_with_nested(
            ParsingException( "Could not parse param " + _paramName, __CLASS_NAME__ ) );
    }
}


node_id Node::getNodeID() const {
    return nodeID;
}


ptr< ProposalHashDB > Node::getProposalHashDB() const {
    CHECK_STATE( proposalHashDB );
    return proposalHashDB;
}

ptr< ProposalVectorDB > Node::getProposalVectorDB() const {
    CHECK_STATE( proposalVectorDB );
    return proposalVectorDB;
}

ptr< MsgDB > Node::getOutgoingMsgDB() const {
    CHECK_STATE( outgoingMsgDB );
    return outgoingMsgDB;
}

ptr< MsgDB > Node::getIncomingMsgDB() const {
    CHECK_STATE( incomingMsgDB );
    return incomingMsgDB;
}

ptr< ConsensusStateDB > Node::getConsensusStateDB() const {
    CHECK_STATE( consensusStateDB );
    return consensusStateDB;
}

ptr< map< uint64_t, ptr< NodeInfo > > > Node::getNodeInfosByIndex() const {
    CHECK_STATE( nodeInfosByIndex );
    return nodeInfosByIndex;
}


ptr< Network > Node::getNetwork() const {
    if ( !network ) {
        CHECK_STATE( network );
    }
    return network;
}

nlohmann::json Node::getCfg() const {
    return cfg;
}


Sockets* Node::getSockets() const {
    CHECK_STATE( sockets );
    return sockets.get();
}

Schain* Node::getSchain() const {
    CHECK_STATE( sChain );
    return sChain.get();
}


ptr< SkaleLog > Node::getLog() const {
    CHECK_STATE( log );
    return log;
}


string Node::getBindIP() const {
    CHECK_STATE( !bindIP.empty() )
    return bindIP;
}

network_port Node::getBasePort() const {
    CHECK_STATE( basePort > 0 );
    return basePort;
}


ptr< NodeInfo > Node::getNodeInfoByIndex( schain_index _index ) {
    CHECK_STATE( nodeInfosByIndex );

    if ( nodeInfosByIndex->count( ( uint64_t ) _index ) == 0 )
        return nullptr;
    return nodeInfosByIndex->at( ( uint64_t ) _index );
}


ptr< NodeInfo > Node::getNodeInfoById( node_id _id ) {
    CHECK_STATE( nodeInfosById );

    if ( nodeInfosById->count( ( uint64_t ) _id ) == 0 )
        return nullptr;

    return nodeInfosById->at( ( uint64_t ) _id );
}


ptr< BlockDB > Node::getBlockDB() const {
    CHECK_STATE( blockDB );
    return blockDB;
}

ptr< RandomDB > Node::getRandomDB() const {
    CHECK_STATE( randomDB );
    return randomDB;
}

ptr< PriceDB > Node::getPriceDB() const {
    CHECK_STATE( priceDB )
    return priceDB;
}

ptr< InternalInfoDB > Node::getInternalInfoDB() const {
    CHECK_STATE( internalInfoDB )
    return internalInfoDB;
}


uint64_t Node::getCatchupIntervalMs() {
    return catchupIntervalMS;
}

uint64_t Node::getMonitoringIntervalMs() {
    return monitoringIntervalMs;
}

uint64_t Node::getStuckMonitoringIntervalMs() {
    return stuckMonitoringIntervalMs;
}

uint64_t Node::getStuckRestartIntervalMs() {
    return stuckRestartIntervalMs;
}


uint64_t Node::getWaitAfterNetworkErrorMs() {
    return waitAfterNetworkErrorMs;
}

uint64_t Node::getEmptyBlockIntervalMs() const {
    return emptyBlockIntervalMs;
}

uint64_t Node::getMaxCatchupDownloadBytes() const {
    return maxCatchupDownloadBytes;
}


uint64_t Node::getMaxTransactionsPerBlock() const {
    return maxTransactionsPerBlock;
}

uint64_t Node::getMinBlockIntervalMs() const {
    return minBlockIntervalMs;
}

uint64_t Node::getBlockDBSize() const {
    return blockDBSize;
}

uint64_t Node::getConsensusStateDBSize() const {
    return consensusStateDBSize;
}


void Node::setBasePort( const network_port& _basePort ) {
    CHECK_ARGUMENT( _basePort > 0 );
    basePort = _basePort;
}

uint64_t Node::getSimulateNetworkWriteDelayMs() const {
    return simulateNetworkWriteDelayMs;
}

const ptr< TestConfig >& Node::getTestConfig() const {
    CHECK_STATE( testConfig )
    return testConfig;
}

bool Node::isExitRequested() {
    return exitRequested;
}

bool Node::isStarted() const {
    return startedServers;
}

uint64_t Node::getRandomDBSize() const {
    return randomDBSize;
}

uint64_t Node::getPriceDBSize() const {
    return priceDBSize;
}

uint64_t Node::getBlockSigShareDBSize() const {
    return blockSigShareDBSize;
}

ptr< BlockSigShareDB > Node::getBlockSigShareDB() const {
    CHECK_STATE( blockSigShareDB );
    return blockSigShareDB;
}

ptr< DASigShareDB > Node::getDaSigShareDB() const {
    CHECK_STATE( daSigShareDB );
    return daSigShareDB;
}

ptr< DAProofDB > Node::getDaProofDB() const {
    CHECK_STATE( daProofDB );
    return daProofDB;
}

uint64_t Node::getDaSigShareDBSize() const {
    return daSigShareDBSize;
}

uint64_t Node::getDaProofDBSize() const {
    return daProofDBSize;
}

ptr< BlockProposalDB > Node::getBlockProposalDB() const {
    CHECK_STATE( blockProposalDB )
    return blockProposalDB;
}

uint64_t Node::getBlockProposalDBSize() const {
    return blockProposalDBSize;
}

uint64_t Node::getInternalInfoDBSize() const {
    return internalInfoDBSize;
}

map< string, uint64_t > Node::getDBUsage() const {
    map< string, uint64_t > ret;
    ret["blocks.db_disk_usage"] = getBlockDB()->getActiveDBSize();
    ret["block_proposal.db_disk_usage"] = getBlockProposalDB()->getActiveDBSize();
    ret["block_sigshare.db_disk_usage"] = getBlockSigShareDB()->getActiveDBSize();
    ret["consensus_state.db_disk_usage"] = getConsensusStateDB()->getActiveDBSize();
    ret["da_proof.db_disk_usage"] = getDaProofDB()->getActiveDBSize();
    ret["da_sigshare.db_disk_usage"] = getDaSigShareDB()->getActiveDBSize();
    ret["incoming_msg.db_disk_usage"] = getIncomingMsgDB()->getActiveDBSize();
    ret["interna_info.db_disk_usage"] = getInternalInfoDB()->getActiveDBSize();
    ret["outgoing_msg.db_disk_usage"] = getOutgoingMsgDB()->getActiveDBSize();
    ret["price.db_disk_usage"] = getPriceDB()->getActiveDBSize();
    ret["proposal_hash.db_disk_usage"] = getProposalHashDB()->getActiveDBSize();
    ret["proposal_vector.db_disk_usage"] = getProposalVectorDB()->getActiveDBSize();
    ret["random.db_disk_usage"] = getRandomDB()->getActiveDBSize();

    return ret;
}

ConsensusEngine* Node::getConsensusEngine() const {
    CHECK_STATE( consensusEngine );
    return consensusEngine;
}
string Node::getSgxUrl() {
    CHECK_STATE( !sgxURL.empty() );
    return sgxURL;
}
string Node::getSgxSslKeyFileFullPath() {
    return sgxSSLKeyFileFullPath;
}

string Node::getSgxSslCertFileFullPath() {
    return sgxSSLCertFileFullPath;
}

uint64_t Node::getVisualizationType() const {
    return visualizationType;
}

bool Node::isTestNet() const {
    return testNet;
}

void Node::setExitOnBlockBoundaryRequested() {
    LOG( info, "Set exit on block boundary" );
    exitOnBlockBoundaryRequested = true;
}
