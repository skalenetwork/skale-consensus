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

    @file CatchupServerAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"

#include "crypto/BLAKE3Hash.h"

#include "exceptions/ExitRequestedException.h"
#include "exceptions/InvalidMessageFormatException.h"
#include "exceptions/PingException.h"

#include "monitoring/LivelinessMonitor.h"


#include "node/NodeInfo.h"


#include "db/BlockDB.h"
#include "db/DAProofDB.h"

#include "abstracttcpserver/ConnectionStatus.h"

#include "exceptions/CouldNotSendMessageException.h"
#include "exceptions/InvalidNodeIDException.h"
#include "exceptions/InvalidSchainException.h"
#include "exceptions/OldBlockIDException.h"


#include "pendingqueue/PendingTransactionsAgent.h"

#include "chains/Schain.h"
#include "headers/BlockFinalizeResponseHeader.h"
#include "headers/BlockProposalHeader.h"
#include "headers/CatchupRequestHeader.h"
#include "headers/CatchupResponseHeader.h"
#include "network/IO.h"
#include "network/Network.h"
#include "network/ServerConnection.h"
#include "network/Sockets.h"

#include "CatchupServerAgent.h"
#include "datastructures/BlockProposalFragment.h"
#include "datastructures/CommittedBlock.h"


CatchupServerAgent::CatchupServerAgent(Schain &_schain, const ptr<TCPServerSocket>& _s) : AbstractServerAgent(
        "CatchupServer", _schain, _s) {
    CHECK_ARGUMENT(_s);
    catchupWorkerThreadPool = make_shared<CatchupWorkerThreadPool>(num_threads(2), this);
    catchupWorkerThreadPool->startService();
    createNetworkReadThread();
}

CatchupServerAgent::~CatchupServerAgent() {

}


void CatchupServerAgent::processNextAvailableConnection(const ptr<ServerConnection>& _connection) {


    MONITOR(__CLASS_NAME__, __FUNCTION__);

    CHECK_ARGUMENT(_connection);

    try {
        sChain->getIo()->readMagic(_connection->getDescriptor());
    }
    catch (PingException &) { throw; }
    catch (ExitRequestedException &) { throw; }
    catch (...) {
        throw_with_nested(NetworkProtocolException("Incorrect magic number", __CLASS_NAME__));
    }


    nlohmann::json jsonRequest = nullptr;

    try {
        jsonRequest = sChain->getIo()->readJsonHeader(_connection->getDescriptor(), "Read catchup request",
            10,
            _connection->getIP());
    }
    catch (ExitRequestedException &) { throw; }
    catch (...) {
        throw_with_nested(NetworkProtocolException("Could not read request", __CLASS_NAME__));
    }


    ptr<Header> responseHeader = nullptr;

    auto type = Header::getString(jsonRequest, "type");


    if (type.compare(Header::BLOCK_CATCHUP_REQ) == 0) {
        responseHeader = make_shared<CatchupResponseHeader>();
    } else if (type.compare(Header::BLOCK_FINALIZE_REQ) == 0) {
        responseHeader = make_shared<BlockFinalizeResponseHeader>();
    } else {
        BOOST_THROW_EXCEPTION(
                InvalidMessageFormatException("Unknown request type:" + type, __CLASS_NAME__));
    }

    ptr<vector<uint8_t>> serializedBinary = nullptr;

    try {
        serializedBinary = this->createResponseHeaderAndBinary(_connection, jsonRequest, responseHeader);
    }
    catch (ExitRequestedException &) { throw; }
    catch (...) {
        try {
            responseHeader->setStatusSubStatus(CONNECTION_ERROR, CONNECTION_SUBSTATUS_UNKNOWN);
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


    if (serializedBinary == nullptr) {
        LOG(debug, "Server step 3: response completed: no blocks sent");
        return;
    }

    try {
        getSchain()->getIo()->writeBytesVector(_connection->getDescriptor(), serializedBinary);
    } catch (ExitRequestedException &) {
        throw;
    }
    catch (...) {
        throw_with_nested(CouldNotSendMessageException("Could not send serialized binary", __CLASS_NAME__));
    }

    LOG(debug, "Server step 3: response completed: blocks sent");

    return;


}


ptr<vector<uint8_t>> CatchupServerAgent::createResponseHeaderAndBinary(const ptr<ServerConnection>& ,
                                                                       nlohmann::json _jsonRequest,
                                                                       const ptr<Header> &_responseHeader) {

    CHECK_ARGUMENT(_responseHeader);

    try {

        schain_id schainID = Header::getUint64(_jsonRequest, "schainID");
        block_id blockID = Header::getUint64(_jsonRequest, "blockID");
        node_id nodeID = Header::getUint64(_jsonRequest, "nodeID");


        if ((uint64_t ) sChain->getSchainID() != schainID) {
            _responseHeader->setStatusSubStatus(CONNECTION_ERROR, CONNECTION_ERROR_UNKNOWN_SCHAIN_ID);
            BOOST_THROW_EXCEPTION(InvalidSchainException("Incorrect schain " + to_string(schainID), __CLASS_NAME__));

        };


        ptr<NodeInfo> nmi = sChain->getNode()->getNodeInfoById(nodeID);

        if (nmi == nullptr) {
            _responseHeader->setStatusSubStatus(CONNECTION_ERROR, CONNECTION_ERROR_DONT_KNOW_THIS_NODE);
            BOOST_THROW_EXCEPTION(
                    InvalidNodeIDException("Could not find node info for NODE_ID:" + to_string((uint64_t) nodeID),
                            __CLASS_NAME__));
        }

        auto type = Header::getString(_jsonRequest, "type");

        ptr<vector<uint8_t>> serializedBinary = nullptr;

        if (type.compare(Header::BLOCK_CATCHUP_REQ) == 0) {

            serializedBinary = createBlockCatchupResponse(_jsonRequest,
                                                          dynamic_pointer_cast<CatchupResponseHeader>(_responseHeader),
                                                          blockID);

        } else if (type.compare(Header::BLOCK_FINALIZE_REQ) == 0) {

            serializedBinary = createBlockFinalizeResponse(_jsonRequest,
                                                           dynamic_pointer_cast<BlockFinalizeResponseHeader>(
                                                                   _responseHeader), blockID);

        }


        return serializedBinary;
    }
    catch (ExitRequestedException &e) { throw; } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }


}


ptr<vector<uint8_t>> CatchupServerAgent::createBlockCatchupResponse(nlohmann::json /*_jsonRequest */,
                                                                    const ptr<CatchupResponseHeader>& _responseHeader,
                                                                    block_id _blockID) {

    CHECK_ARGUMENT(_responseHeader);

    MONITOR(__CLASS_NAME__, __FUNCTION__);

    try {

        if (sChain->getLastCommittedBlockID() <= block_id(_blockID)) {
            LOG(debug, "Catchups: sChain->getCommittedBlockID() <= block_id(blockID)");
            _responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT, CONNECTION_NO_NEW_BLOCKS);
            _responseHeader->setComplete();
            return nullptr;
        }


        auto blockSizes = make_shared<list<uint64_t>>();

        auto committedBlockID = sChain->getLastCommittedBlockID();

        if (_blockID >= committedBlockID) {
            LOG(debug, "Catchups: blockID >= committedBlockID");
            _responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT, CONNECTION_OK);
            _responseHeader->setComplete();
            return nullptr;
        }

        auto serializedBlocks = make_shared<vector<uint8_t>>();

        serializedBlocks->push_back('[');

        uint64_t totalSize = 0;

        auto maxSize = getSchain()->getNode()->getMaxCatchupDownloadBytes();

        for (uint64_t i = (uint64_t) _blockID + 1; i <= committedBlockID; i++) {

            auto serializedBlock = getSchain()->getNode()->getBlockDB()->getSerializedBlockFromLevelDB(i);

            if (serializedBlock == nullptr) {
                _responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT, CONNECTION_CATCHUP_DONT_HAVE_THIS_BLOCK );
                _responseHeader->setComplete();
                return nullptr;
            }

            totalSize += serializedBlock->size();

            if (totalSize > maxSize)
                break;

            serializedBlocks->insert(serializedBlocks->end(), serializedBlock->begin(), serializedBlock->end());

            blockSizes->push_back(serializedBlock->size());


        }

        serializedBlocks->push_back(']');

        _responseHeader->setStatusSubStatus(CONNECTION_PROCEED, CONNECTION_OK);

        _responseHeader->setBlockSizes(blockSizes);

        return serializedBlocks;
    } catch (ExitRequestedException &e) { throw; } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


ptr<vector<uint8_t>> CatchupServerAgent::createBlockFinalizeResponse(nlohmann::json _jsonRequest,
                                                                     const ptr<BlockFinalizeResponseHeader>& _responseHeader,
                                                                     block_id _blockID) {

    CHECK_ARGUMENT(_responseHeader);

    MONITOR(__CLASS_NAME__, __FUNCTION__);

    try {
        fragment_index fragmentIndex = Header::getUint64(_jsonRequest, "fragmentIndex");

        if (fragmentIndex < 1 || (uint64_t) fragmentIndex > getSchain()->getNodeCount() - 1) {
            LOG(debug, "Incorrect fragment index:" + to_string(fragmentIndex));
            _responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT, CONNECTION_ERROR_INVALID_FRAGMENT_INDEX);
            _responseHeader->setComplete();
            return nullptr;
        }


        schain_index proposerIndex = Header::getUint64(_jsonRequest, "proposerIndex");


        if (proposerIndex < 1 || (uint64_t) fragmentIndex > getSchain()->getNodeCount()) {
            LOG(debug, "Incorrect proposer index:" + to_string(proposerIndex));
            _responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT, CONNECTION_ERROR_INVALID_PROPOSER_INDEX);
            _responseHeader->setComplete();
            return nullptr;
        }


        auto proposal = getSchain()->getBlockProposal(_blockID, proposerIndex);

        if (proposal == nullptr) {
            LOG(debug, "No proposal in finalization:" + to_string(proposerIndex));


            auto committedBlock = getSchain()->getBlock(_blockID);

            if (committedBlock && committedBlock->getProposerIndex() == (uint64_t ) proposerIndex) {
                proposal = committedBlock;
            } else {
                _responseHeader->setStatusSubStatus(
                    CONNECTION_DISCONNECT, CONNECTION_FINALIZE_DONT_HAVE_PROPOSAL );
                _responseHeader->setComplete();
                return nullptr;
            }
        }

        if (!getNode()->getDaProofDB()->haveDAProof(proposal)) {
            LOG(trace, "Dont have DA proof:" + to_string(proposerIndex));
            _responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT,
                                                CONNECTION_DONT_HAVE_DA_PROOF_FOR_PROPOSAL);
            _responseHeader->setComplete();
            return nullptr;
        }

        auto fragment = proposal->getFragment(
                (uint64_t) getSchain()->getNodeCount() - 1,
                fragmentIndex);

        CHECK_STATE(fragment != nullptr);

        _responseHeader->setStatusSubStatus(CONNECTION_PROCEED, CONNECTION_OK);

        auto serializedFragment = fragment->serialize();

        CHECK_STATE(serializedFragment);

        _responseHeader->setFragmentParams(serializedFragment->size(),
                                           proposal->serialize()->size(), proposal->getHash()->toHex());

        return serializedFragment;
    } catch (ExitRequestedException &e) { throw; } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}
