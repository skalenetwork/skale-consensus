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

    @file CatchupServerAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include <leveldb/options.h>
#include "../../SkaleCommon.h"
#include "../../Log.h"

#include "leveldb/db.h"

#include "../../exceptions/FatalError.h"
#include "../../exceptions/ExitRequestedException.h"
#include "../../exceptions/PingException.h"
#include "../../node/ConsensusEngine.h"
#include "../../thirdparty/json.hpp"


#include "../../node/NodeInfo.h"
#include "../../abstracttcpserver/ConnectionStatus.h"

#include "../../exceptions/OldBlockIDException.h"
#include "../../exceptions/InvalidSchainException.h"
#include "../../exceptions/InvalidSourceIPException.h"
#include "../../exceptions/InvalidNodeIDException.h"
#include "../../exceptions/InvalidSchainIndexException.h"
#include "../../exceptions/CouldNotSendMessageException.h"


#include "../../pendingqueue/PendingTransactionsAgent.h"

#include "../../chains/Schain.h"
#include "../../network/TransportNetwork.h"
#include "../../network/Sockets.h"
#include "../../network/Connection.h"
#include "../../network/IO.h"
#include "../../headers/CatchupRequestHeader.h"
#include "../../headers/CommittedBlockHeader.h"
#include "../../headers/CatchupResponseHeader.h"

#include "../../datastructures/CommittedBlock.h"
#include "../../datastructures/CommittedBlockList.h"
#include "CatchupServerAgent.h"


CatchupWorkerThreadPool *CatchupServerAgent::getCatchupWorkerThreadPool() const {
    return catchupWorkerThreadPool.get();
}


CatchupServerAgent::CatchupServerAgent(Schain &_schain, ptr<TCPServerSocket> _s) : AbstractServerAgent(
        "Block proposal server", _schain, _s) {
    catchupWorkerThreadPool = make_shared<CatchupWorkerThreadPool>(num_threads(1), this);
    catchupWorkerThreadPool->startService();
    createNetworkReadThread();
}

CatchupServerAgent::~CatchupServerAgent() {

}


void CatchupServerAgent::processNextAvailableConnection(ptr<Connection> _connection) {


    try {
        sChain->getIo()->readMagic(_connection->getDescriptor());
    }
    catch (PingException &) { return; }
    catch (ExitRequestedException &) { throw; }
    catch (...) {
        throw_with_nested(NetworkProtocolException("Incorrect magic number", __CLASS_NAME__));
    }


    nlohmann::json catchupRequest = nullptr;

    try {
        catchupRequest = sChain->getIo()->readJsonHeader(_connection->getDescriptor(), "Read catchup request");
    }
    catch (ExitRequestedException &) { throw; }
    catch (...) {
        throw_with_nested(NetworkProtocolException("Incorrect magic number", __CLASS_NAME__));
    }


    auto responseHeader = make_shared<CatchupResponseHeader>();


    ptr<vector<uint8_t>> serializedBlocks = nullptr;

    try {
        serializedBlocks = this->createCatchupResponseHeader(_connection, catchupRequest, responseHeader);
    }
    catch (ExitRequestedException &) { throw; }
    catch (...) {
        try {
            responseHeader->setStatus(CONNECTION_SERVER_ERROR);
            responseHeader->setComplete();
            send(_connection, responseHeader);
        } catch (ExitRequestedException &) {
            throw;
        } catch (...) {}
        throw_with_nested(CouldNotSendMessageException("Could not create catchup response header", __CLASS_NAME__));
    }


    try {
        send(_connection, responseHeader);
    }
    catch (ExitRequestedException &) {
        throw;
    }
    catch (...) {
        throw_with_nested(CouldNotSendMessageException("Could not send response", __CLASS_NAME__));
    }


    LOG(debug, "Server step 2: sent catchup response header");


    if (serializedBlocks == nullptr) {
        LOG(debug, "Server step 3: response completed: no missing blocks");
        return;
    }

    try {
        getSchain()->getIo()->writeBytesVector(_connection->getDescriptor(), serializedBlocks);
    } catch (ExitRequestedException &) {
        throw;
    }
    catch (...) {
        throw_with_nested(CouldNotSendMessageException("Could not send raw blocks", __CLASS_NAME__));
    }

    LOG(debug, "Server step 3: response completed with missing blocks");

    return;


}


ptr<vector<uint8_t>> CatchupServerAgent::createCatchupResponseHeader(ptr<Connection> _connectionEnvelope,
                                                                     nlohmann::json _jsonRequest,
                                                                     ptr<CatchupResponseHeader> _responseHeader) {


    schain_id schainID = Header::getUint64(_jsonRequest, "schainID");
    node_id srcNodeID = Header::getUint64(_jsonRequest, "srcNodeID");
    schain_index srcSchainIndex = Header::getUint64(_jsonRequest, "srcSchainIndex");
    block_id blockID = Header::getUint64(_jsonRequest, "blockID");


    LOG(debug, "Catchups started: got request block:" + to_string(blockID));


    if (sChain->getSchainID() != schainID) {
        _responseHeader->setStatusSubStatus(CONNECTION_SERVER_ERROR, CONNECTION_ERROR_UNKNOWN_SCHAIN_ID);

        BOOST_THROW_EXCEPTION(InvalidSchainException("Incorrect schain " + to_string(schainID), __CLASS_NAME__));

    };


    ptr<NodeInfo> nmi = sChain->getNode()->getNodeInfoByIP(_connectionEnvelope->getIP());

    if (nmi == nullptr) {
        _responseHeader->setStatusSubStatus(CONNECTION_SERVER_ERROR, CONNECTION_ERROR_DONT_KNOW_THIS_NODE);
        BOOST_THROW_EXCEPTION(
                InvalidSourceIPException("Could not find node info for IP " + *_connectionEnvelope->getIP()));
    }


    if (nmi->getNodeID() != node_id(srcNodeID)) {
        _responseHeader->setStatusSubStatus(CONNECTION_SERVER_ERROR, CONNECTION_ERROR_INVALID_NODE_ID);
        BOOST_THROW_EXCEPTION(InvalidNodeIDException("Node ID does not match " + srcNodeID, __CLASS_NAME__));
    }

    if (nmi->getSchainIndex() - 1 != schain_index(srcSchainIndex)) { /// XXXX
        _responseHeader->setStatusSubStatus(CONNECTION_SERVER_ERROR, CONNECTION_ERROR_INVALID_NODE_INDEX);
        BOOST_THROW_EXCEPTION(
                InvalidSchainIndexException("Node subchain index does not match " + srcSchainIndex, __CLASS_NAME__));
    }


    if (sChain->getCommittedBlockID() <= block_id(blockID)) {
        LOG(debug, "Catchups: sChain->getCommittedBlockID() <= block_id(blockID)");
        _responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT, CONNECTION_NO_NEW_BLOCKS);
        _responseHeader->setComplete();
        return nullptr;
    }

    auto blockSizes = make_shared<list<uint64_t>>();

    auto committedBlockID = sChain->getCommittedBlockID();

    if (blockID >= committedBlockID) {
        LOG(debug, "Catchups: blockID >= committedBlockID");
        _responseHeader->setStatus(CONNECTION_DISCONNECT);
        _responseHeader->setComplete();
        return nullptr;
    }

    auto serializedBlocks = make_shared<vector<uint8_t>>();



    for (uint64_t i = (uint64_t) blockID + 1; i <= committedBlockID; i++) {

        auto serializedBlock = getSerializedBlock(i);

        if (!serializedBlock) {
            _responseHeader->setStatus(CONNECTION_DISCONNECT);
            _responseHeader->setComplete();
            return nullptr;
        }

        serializedBlocks->insert(serializedBlocks->end(), serializedBlock->begin(), serializedBlock->end());

        blockSizes->push_back(serializedBlock->size());

    }

    _responseHeader->setStatus(CONNECTION_PROCEED);

    _responseHeader->setBlockSizes(blockSizes);

    _responseHeader->setComplete();

    LOG(debug, "Catchup completed successfully");

    return serializedBlocks;

}

ptr<vector<uint8_t>> CatchupServerAgent::getSerializedBlock(uint64_t i) const {


    auto block = sChain->getCachedBlock(i);


    if (block) {
        return block->serialize();
    } else {
        return sChain->getSerializedBlockFromLevelDB(i);
    }

}


