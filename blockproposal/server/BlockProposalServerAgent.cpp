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

    @file BlockProposalServerAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../../Agent.h"
#include "../../SkaleCommon.h"
#include "../../Log.h"
#include "../../exceptions/FatalError.h"
#include "../../thirdparty/json.hpp"

#include "../../utils/Time.h"
#include "../../abstracttcpserver/ConnectionStatus.h"
#include "../../exceptions/CouldNotReadPartialDataHashesException.h"
#include "../../exceptions/CouldNotSendMessageException.h"
#include "../../exceptions/ExitRequestedException.h"
#include "../../exceptions/InvalidHashException.h"
#include "../../exceptions/InvalidNodeIDException.h"
#include "../../exceptions/InvalidSchainException.h"
#include "../../exceptions/InvalidSchainIndexException.h"
#include "../../exceptions/InvalidSourceIPException.h"
#include "../../exceptions/OldBlockIDException.h"
#include "../../exceptions/PingException.h"
#include "../../node/NodeInfo.h"

#include "../../crypto/ConsensusBLSSigShare.h"


#include "../../pendingqueue/PendingTransactionsAgent.h"
#include "../pusher/BlockProposalClientAgent.h"
#include "../received/ReceivedBlockProposalsDatabase.h"

#include "../../abstracttcpserver/AbstractServerAgent.h"
#include "../../chains/Schain.h"
#include "../../crypto/SHAHash.h"
#include "../../headers/BlockProposalResponseHeader.h"
#include "../../headers/Header.h"
#include "../../headers/MissingTransactionsRequestHeader.h"
#include "../../network/ServerConnection.h"
#include "../../network/IO.h"
#include "../../network/Sockets.h"
#include "../../network/TransportNetwork.h"

#include "../../node/Node.h"

#include "../../datastructures/BlockProposal.h"
#include "../../datastructures/CommittedBlock.h"
#include "../../datastructures/PartialHashesList.h"
#include "../../datastructures/ReceivedBlockProposal.h"
#include "../../datastructures/Transaction.h"
#include "../../datastructures/TransactionList.h"
#include "../../headers/AbstractBlockRequestHeader.h"
#include "../../headers/BlockProposalHeader.h"


#include "../../crypto/ConsensusBLSSigShare.h"
#include "../../headers/BlockFinalizeResponseHeader.h"
#include "BlockProposalServerAgent.h"
#include "BlockProposalWorkerThreadPool.h"


ptr<unordered_map<ptr<partial_sha_hash>, ptr<Transaction>, PendingTransactionsAgent::Hasher, PendingTransactionsAgent::Equal> >
BlockProposalServerAgent::readMissingTransactions(ptr<ServerConnection> connectionEnvelope_,
                                                  nlohmann::json missingTransactionsResponseHeader) {
    ASSERT(missingTransactionsResponseHeader > 0);

    auto transactionSizes = make_shared<vector<uint64_t> >();

    nlohmann::json jsonSizes = missingTransactionsResponseHeader["sizes"];

    if (!jsonSizes.is_array()) {
        BOOST_THROW_EXCEPTION(NetworkProtocolException("jsonSizes is not an array", __CLASS_NAME__));
    };


    size_t totalSize = 2;  // account for starting and ending < >

    for (auto &&size : jsonSizes) {
        transactionSizes->push_back(size);
        totalSize += (size_t) size;
    }

    auto serializedTransactions = make_shared<vector<uint8_t> >(totalSize);


    try {
        getSchain()->getIo()->readBytes(connectionEnvelope_, (in_buffer *) serializedTransactions->data(),
                                        msg_len(totalSize));
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        BOOST_THROW_EXCEPTION(NetworkProtocolException("Could not read serialized exceptions", __CLASS_NAME__));
    }

    auto list = TransactionList::deserialize(transactionSizes, serializedTransactions, 0, false);

    auto trs = list->getItems();

    auto missed = make_shared<unordered_map<ptr<partial_sha_hash>, ptr<Transaction>, PendingTransactionsAgent::Hasher, PendingTransactionsAgent::Equal> >();

    for (auto &&t : *trs) {
        (*missed)[t->getPartialHash()] = t;
    }

    return missed;
}


BlockProposalWorkerThreadPool *BlockProposalServerAgent::getBlockProposalWorkerThreadPool() const {
    return blockProposalWorkerThreadPool.get();
}


pair<ptr<map<uint64_t, ptr<Transaction> > >, ptr<map<uint64_t, ptr<partial_sha_hash> > > >
BlockProposalServerAgent::getPresentAndMissingTransactions(Schain &_sChain, ptr<Header> /*tcpHeader*/,
                                                           ptr<PartialHashesList> _phm) {
    LOG(debug, "Calculating missing hashes");

    auto transactionsCount = _phm->getTransactionCount();

    auto presentTransactions = make_shared<map<uint64_t, ptr<Transaction> > >();
    auto missingHashes = make_shared<map<uint64_t, ptr<partial_sha_hash> > >();

    for (uint64_t i = 0; i < transactionsCount; i++) {
        auto hash = _phm->getPartialHash(i);
        ASSERT(hash);
        auto transaction = _sChain.getPendingTransactionsAgent()->getKnownTransactionByPartialHash(hash);
        if (transaction == nullptr) {
            (*missingHashes)[i] = hash;
        } else {
            (*presentTransactions)[i] = transaction;
        }
    }

    return {presentTransactions, missingHashes};
}


BlockProposalServerAgent::BlockProposalServerAgent(Schain &_schain, ptr<TCPServerSocket> _s) : AbstractServerAgent(
        "BlockPropSrv", _schain, _s) {
    blockProposalWorkerThreadPool = make_shared<BlockProposalWorkerThreadPool>(num_threads(1), this);
    blockProposalWorkerThreadPool->startService();
    createNetworkReadThread();
}

BlockProposalServerAgent::~BlockProposalServerAgent() {}


void BlockProposalServerAgent::processNextAvailableConnection(ptr<ServerConnection> _connection) {
    try {
        sChain->getIo()->readMagic(_connection->getDescriptor());
    } catch (ExitRequestedException &) {
        throw;
    } catch (PingException &) {
        return;
    } catch (...) {
        throw_with_nested(NetworkProtocolException("Could not read magic number", __CLASS_NAME__));
    }


    nlohmann::json proposalRequest = nullptr;

    try {
        proposalRequest = getSchain()->getIo()->readJsonHeader(_connection->getDescriptor(), "Read proposal req");
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(CouldNotSendMessageException("Could not read proposal request", __CLASS_NAME__));
    }


    auto type = Header::getString(proposalRequest, "type");

    if (strcmp(type->data(), Header::BLOCK_PROPOSAL_REQ) == 0) {
        processProposalRequest(_connection, proposalRequest);
    } else {
        BOOST_THROW_EXCEPTION(NetworkProtocolException("Uknown request type:" + *type, __CLASS_NAME__));
    }
}

void
BlockProposalServerAgent::processProposalRequest(ptr<ServerConnection> _connection, nlohmann::json _proposalRequest) {
    ptr<Header> responseHeader = nullptr;

    ptr<BlockProposalHeader> header = nullptr;
    try {
        auto schainID = (schain_id)Header::getUint64(_proposalRequest, "schainID");
        auto blockID = (block_id) Header::getUint64(_proposalRequest, "blockID");
        auto proposerNodeID = (node_id) Header::getUint64(_proposalRequest, "proposerNodeID");
        auto proposerIndex = (schain_index) Header::getUint64(_proposalRequest, "proposerIndex");
        auto timeStamp = Header::getUint64(_proposalRequest, "timeStamp");
        auto timeStampMs = Header::getUint32(_proposalRequest, "timeStampMs");
        auto hash = Header::getString(_proposalRequest, "hash");
        auto signature = Header::getString(_proposalRequest, "sig");
        auto  txCount = Header::getUint64(_proposalRequest, "txCount");



        header = make_shared<BlockProposalHeader>(getSchain()->getNodeCount(), schainID,
                                   blockID, proposerIndex, proposerNodeID,
                                   make_shared<string>(*hash),
                                   make_shared<string>(*signature), txCount, timeStamp, timeStampMs);
        responseHeader = this->createProposalResponseHeader(_connection, *header);

        send(_connection, responseHeader);

    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(NetworkProtocolException("Could not create response header", __CLASS_NAME__));
    }

    if (responseHeader->getStatus() != CONNECTION_PROCEED) {
        return;
    }

    ptr<PartialHashesList> partialHashesList = nullptr;

    try {
        partialHashesList = readPartialHashes(_connection, header->getTxCount());
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(NetworkProtocolException("Could not read partial hashes", __CLASS_NAME__));
    }




    auto result = getPresentAndMissingTransactions(*sChain, responseHeader, partialHashesList);

    auto presentTransactions = result.first;
    auto missingTransactionHashes = result.second;

    auto missingHashesRequestHeader = make_shared<MissingTransactionsRequestHeader>(missingTransactionHashes);

    try {
        send(_connection, missingHashesRequestHeader);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(CouldNotSendMessageException("Could not send missing hashes request header", __CLASS_NAME__));
    }


    ptr<unordered_map<ptr<partial_sha_hash>, ptr<Transaction>, PendingTransactionsAgent::Hasher, PendingTransactionsAgent::Equal> > missingTransactions = nullptr;

    if (missingTransactionHashes->size() == 0) {
        LOG(debug, "Server: No missing partial hashes");
    } else {
        LOG(debug, "Server: missing partial hashes");
        try {
            getSchain()->getIo()->writePartialHashes(_connection->getDescriptor(), missingTransactionHashes);
        } catch (ExitRequestedException &) {
            throw;
        } catch (...) {
            BOOST_THROW_EXCEPTION(CouldNotSendMessageException(
                                          "Could not send missing hashes request header", __CLASS_NAME__ ));
        }


        auto missingMessagesResponseHeader = this->readMissingTransactionsResponseHeader(_connection);

        missingTransactions = readMissingTransactions(_connection, missingMessagesResponseHeader);


        if (missingTransactions == nullptr) {
            BOOST_THROW_EXCEPTION(CouldNotReadPartialDataHashesException(
                                          "Null missing transactions", __CLASS_NAME__ ));
        }


        for (auto &&item : *missingTransactions) {
            sChain->getPendingTransactionsAgent()->pushKnownTransaction(item.second);
        }
    }


    if (sChain->getLastCommittedBlockID() >= header->getBlockId())
        return;

    LOG(debug, "Storing block proposal");


    auto transactions = make_shared<vector<ptr<Transaction> > >();

    auto transactionCount = partialHashesList->getTransactionCount();

    for (uint64_t i = 0; i < transactionCount; i++) {
        auto h = partialHashesList->getPartialHash(i);
        ASSERT(h);

        if (getSchain()->getPendingTransactionsAgent()->isCommitted(h)) {
            checkForOldBlock(header->getBlockId());
            BOOST_THROW_EXCEPTION(CouldNotReadPartialDataHashesException("Committed transaction", __CLASS_NAME__));
        }

        ptr<Transaction> transaction;

        if (presentTransactions->count(i) > 0) {
            transaction = presentTransactions->at(i);
        } else {
            transaction = (*missingTransactions)[h];
        };


        if (transaction == nullptr) {
            checkForOldBlock(header->getBlockId());
            ASSERT(missingTransactions);

            if (missingTransactions->count(h) > 0) {
                LOG(err, "Found in missing");
            }

            ASSERT(false);
        }


        ASSERT(transactions != nullptr);

        transactions->push_back(transaction);
    }

    ASSERT(transactionCount == 0 || transactions->at((uint64_t) transactionCount - 1));

    ASSERT(header->getTimeStamp() > 0);

    auto transactionList = make_shared<TransactionList>(transactions);

    auto proposal = make_shared<ReceivedBlockProposal>(*sChain, header->getBlockId(),
            header->getProposerIndex(), transactionList, header->getTimeStamp(),
                                                       header->getTimeStampMs());

    auto calculatedHash = proposal->getHash();

    if (calculatedHash->compare(SHAHash::fromHex(header->getHash())) != 0) {
        BOOST_THROW_EXCEPTION(InvalidHashException(
                                      "Block proposal hash does not match" + *proposal->getHash()->toHex() + ":" +
                                      *header->getHash(), __CLASS_NAME__ ));
    }

    sChain->proposedBlockArrived(proposal);
}


void BlockProposalServerAgent::checkForOldBlock(const block_id &_blockID) {
    LOG(debug, "BID:" + to_string(_blockID) + ":CBID:" + to_string(getSchain()->getLastCommittedBlockID()) + ":MQ:" +
               to_string(getSchain()->getMessagesCount()));
    if (_blockID <= getSchain()->getLastCommittedBlockID())
        BOOST_THROW_EXCEPTION(OldBlockIDException("Old block ID", nullptr, nullptr, __CLASS_NAME__));
}


ptr<Header> BlockProposalServerAgent::createProposalResponseHeader(ptr<ServerConnection> _connectionEnvelope,
                                                                   BlockProposalHeader &_header) {
    auto responseHeader = make_shared<BlockProposalResponseHeader>();

    if (sChain->getSchainID() != _header.getSchainId()) {
        responseHeader->setStatusSubStatus(CONNECTION_SERVER_ERROR, CONNECTION_ERROR_UNKNOWN_SCHAIN_ID);
        BOOST_THROW_EXCEPTION(InvalidSchainException("Incorrect schain " + to_string(_header.getSchainId()), __CLASS_NAME__));
    };


    ptr<NodeInfo> nmi = sChain->getNode()->getNodeInfoByIP(_connectionEnvelope->getIP());

    if (nmi == nullptr) {
        responseHeader->setStatusSubStatus(CONNECTION_SERVER_ERROR, CONNECTION_ERROR_DONT_KNOW_THIS_NODE);
        BOOST_THROW_EXCEPTION(InvalidSourceIPException(
                                      "Could not find node info for IP " + *_connectionEnvelope->getIP()));
    }


    if (nmi->getNodeID() != _header.getProposerNodeId()) {
        responseHeader->setStatusSubStatus(CONNECTION_SERVER_ERROR, CONNECTION_ERROR_INVALID_NODE_ID);

        BOOST_THROW_EXCEPTION(InvalidNodeIDException("Node ID does not match " +
           _header.getProposerNodeId(), __CLASS_NAME__));
    }

    if (nmi->getSchainIndex() != schain_index(_header.getProposerIndex())) {
        responseHeader->setStatusSubStatus(CONNECTION_SERVER_ERROR, CONNECTION_ERROR_INVALID_NODE_INDEX);
        BOOST_THROW_EXCEPTION(InvalidSchainIndexException(
                                      "Node schain index does not match " +
                                      _header.getProposerIndex(), __CLASS_NAME__ ));
    }


    if (sChain->getLastCommittedBlockID() >= _header.getBlockId()) {
        responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT, CONNECTION_BLOCK_PROPOSAL_TOO_LATE);
        responseHeader->setComplete();
        return responseHeader;
    }


    ASSERT(_header.getTimeStamp() > MODERN_TIME);

    auto t = Time::getCurrentTimeSec();

    ASSERT(t < (uint64_t) MODERN_TIME * 2);

    if (Time::getCurrentTimeSec() + 1 < _header.getTimeStamp()) {
        LOG(info, "Incorrect timestamp:" + to_string(
                _header.getTimeStamp()) + ":vs:" + to_string(Time::getCurrentTimeSec()));
        responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT, CONNECTION_ERROR_TIME_STAMP_IN_THE_FUTURE);
        responseHeader->setComplete();
        ASSERT(false);
        return responseHeader;
    }


    if (sChain->getLastCommittedBlockTimeStamp() > _header.getTimeStamp()) {
        LOG(info, "Incorrect timestamp:" + to_string(_header.getTimeStamp()) + ":vs:" +
                  to_string(sChain->getLastCommittedBlockTimeStamp()));

        responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT, CONNECTION_ERROR_TIME_STAMP_EARLIER_THAN_COMMITTED);
        responseHeader->setComplete();
        ASSERT(false);
        return responseHeader;
    }


    responseHeader->setStatus(CONNECTION_PROCEED);


    responseHeader->setComplete();
    return responseHeader;
}


nlohmann::json
BlockProposalServerAgent::readMissingTransactionsResponseHeader(ptr<ServerConnection> _connectionEnvelope) {
    auto js = sChain->getIo()->readJsonHeader(_connectionEnvelope->getDescriptor(), "Read missing trans response");

    return js;
}

ptr<PartialHashesList> AbstractServerAgent::readPartialHashes(ptr<ServerConnection> _connectionEnvelope_,
                                                              transaction_count _txCount) {


    if (_txCount > (uint64_t) getNode()->getMaxTransactionsPerBlock()) {
        BOOST_THROW_EXCEPTION(NetworkProtocolException("Too many transactions", __CLASS_NAME__));
    }

    auto partialHashesList = make_shared<PartialHashesList>(_txCount);

    if (_txCount != 0) {
        try {
            getSchain()->getIo()->readBytes(_connectionEnvelope_,
                                            (in_buffer *) partialHashesList->getPartialHashes()->data(),
                                            msg_len((uint64_t) partialHashesList->getTransactionCount() *
                                                    PARTIAL_SHA_HASH_LEN));
        } catch (ExitRequestedException &) { throw; }
        catch (...) {
            throw_with_nested(CouldNotReadPartialDataHashesException("Could not read partial hashes", __CLASS_NAME__));
        }
    }

    return partialHashesList;

}
