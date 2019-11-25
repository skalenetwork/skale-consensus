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
#include "../../crypto/CryptoManager.h"
#include "../../datastructures/DAProof.h"

#include "../../pendingqueue/PendingTransactionsAgent.h"
#include "../pusher/BlockProposalClientAgent.h"
#include "../received/ReceivedBlockProposalsDatabase.h"

#include "../../abstracttcpserver/AbstractServerAgent.h"
#include "../../chains/Schain.h"
#include "../../crypto/SHAHash.h"
#include "../../headers/BlockProposalResponseHeader.h"
#include "../../headers/FinalProposalResponseHeader.h"
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
#include "../../db/ProposalHashDB.h"
#include "../../headers/AbstractBlockRequestHeader.h"
#include "../../headers/BlockProposalHeader.h"
#include "../../headers/DAProofRequestHeader.h"
#include "../../headers/DAProofResponseHeader.h"


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
        getSchain()->getIo()->readBytes(connectionEnvelope_, serializedTransactions,
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


    nlohmann::json clientRequest = nullptr;

    try {
        clientRequest = getSchain()->getIo()->readJsonHeader(_connection->getDescriptor(), "Read proposal req");
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(CouldNotSendMessageException("Could not read proposal request", __CLASS_NAME__));
    }


    auto type = Header::getString(clientRequest, "type");

    if (strcmp(type->data(), Header::BLOCK_PROPOSAL_REQ) == 0) {
        processProposalRequest(_connection, clientRequest);
    } else if  (strcmp(type->data(), Header::DA_PROOF_REQ) == 0) {
        processDAProofRequest(_connection, clientRequest);
    }
    else {
        BOOST_THROW_EXCEPTION(NetworkProtocolException("Uknown request type:" + *type, __CLASS_NAME__));
    }
}


void
BlockProposalServerAgent::processDAProofRequest(ptr<ServerConnection> _connection, nlohmann::json _daProofRequest) {
    ptr<DAProofRequestHeader> requestHeader = nullptr;
    ptr<Header> responseHeader = nullptr;

    try {

        requestHeader = make_shared<DAProofRequestHeader>(_daProofRequest, getSchain()->getNodeCount());
        responseHeader = this->createDAProofResponseHeader(_connection, *requestHeader);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(NetworkProtocolException("Couldnt create DAProof response header", __CLASS_NAME__));
    }

    try {
        send(_connection, responseHeader);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(NetworkProtocolException("Couldnt send daProof response header", __CLASS_NAME__));
    }

    LOG(trace, "Got DA proof");
}

void
BlockProposalServerAgent::processProposalRequest(ptr<ServerConnection> _connection, nlohmann::json _proposalRequest) {
    ptr<BlockProposalHeader> requestHeader = nullptr;
    ptr<Header> responseHeader = nullptr;

    try {

        requestHeader = make_shared<BlockProposalHeader>(_proposalRequest, getSchain()->getNodeCount());
        responseHeader = this->createProposalResponseHeader(_connection, *requestHeader);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(NetworkProtocolException("Couldnt create proposal response header", __CLASS_NAME__));
    }

    try {
        send(_connection, responseHeader);
        if (responseHeader->getStatus() != CONNECTION_PROCEED) {
            return;
        }
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(NetworkProtocolException("Couldnt send proposal response header", __CLASS_NAME__));
    }

    ptr<PartialHashesList> partialHashesList = nullptr;

    try {
        partialHashesList = readPartialHashes(_connection, requestHeader->getTxCount());
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
        throw_with_nested(
                CouldNotSendMessageException("Could not send missing hashes request requestHeader", __CLASS_NAME__));
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
                                          "Could not send missing hashes request requestHeader", __CLASS_NAME__ ));
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

    LOG(debug, "Storing block proposal");

    auto transactions = make_shared<vector<ptr<Transaction> > >();

    auto transactionCount = partialHashesList->getTransactionCount();

    for (uint64_t i = 0; i < transactionCount; i++) {
        auto h = partialHashesList->getPartialHash(i);
        ASSERT(h);


        ptr<Transaction> transaction;

        if (presentTransactions->count(i) > 0) {
            transaction = presentTransactions->at(i);
        } else {
            transaction = (*missingTransactions)[h];
        };

        if (transaction == nullptr) {
            checkForOldBlock(requestHeader->getBlockId());
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
    ASSERT(requestHeader->getTimeStamp() > 0);

    auto transactionList = make_shared<TransactionList>(transactions);

    auto proposal = make_shared<ReceivedBlockProposal>(*sChain, requestHeader->getBlockId(),
                                                       requestHeader->getProposerIndex(), transactionList,
                                                       requestHeader->getTimeStamp(),
                                                       requestHeader->getTimeStampMs(),
                                                       requestHeader->getHash(), requestHeader->getSignature());
    ptr<Header> finalResponseHeader = nullptr;

    try {
        if (!getSchain()->getCryptoManager()->verifyProposalECDSA(proposal.get(), requestHeader->getHash(),
                                                                  requestHeader->getSignature())) {
            finalResponseHeader = make_shared<FinalProposalResponseHeader>(CONNECTION_ERROR,
                                                                           CONNECTION_SIGNATURE_DID_NOT_VERIFY);
        } else {
            finalResponseHeader = this->createFinalResponseHeader(proposal);
        }
        send(_connection, finalResponseHeader);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(NetworkProtocolException("Couldnt create/send final response header", __CLASS_NAME__));
    }


    sChain->proposedBlockArrived(proposal);
}


void BlockProposalServerAgent::checkForOldBlock(const block_id &_blockID) {
    LOG(debug,
        "BID:" + to_string(_blockID) + ":CBID:" + to_string(getSchain()->getLastCommittedBlockID()) + ":MQ:" +
        to_string(getSchain()->getMessagesCount()));
    if (_blockID <= getSchain()->getLastCommittedBlockID())
        BOOST_THROW_EXCEPTION(OldBlockIDException("Old block ID", nullptr, nullptr, __CLASS_NAME__));
}


ptr<Header> BlockProposalServerAgent::createProposalResponseHeader(ptr<ServerConnection> _connectionEnvelope,
                                                                   BlockProposalHeader &_header) {
    auto responseHeader = make_shared<BlockProposalResponseHeader>();

    if (sChain->getSchainID() != _header.getSchainId()) {
        responseHeader->setStatusSubStatus(CONNECTION_SERVER_ERROR, CONNECTION_ERROR_UNKNOWN_SCHAIN_ID);
        BOOST_THROW_EXCEPTION(
                InvalidSchainException("Incorrect schain " + to_string(_header.getSchainId()), __CLASS_NAME__));
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
        return responseHeader;
    }


    if (sChain->getLastCommittedBlockTimeStamp() > _header.getTimeStamp()) {
        LOG(info, "Incorrect timestamp:" + to_string(_header.getTimeStamp()) + ":vs:" +
                  to_string(sChain->getLastCommittedBlockTimeStamp()));

        responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT,
                                           CONNECTION_ERROR_TIME_STAMP_EARLIER_THAN_COMMITTED);
        responseHeader->setComplete();
        return responseHeader;
    }

    if (!getSchain()->getNode()->getProposalHashDb()->checkAndSaveHash(_header.getBlockId(),
                                                                       _header.getProposerIndex(),
                                                                       _header.getHash(),
                                                                       sChain->getLastCommittedBlockID())) {

        LOG(info, "Double proposal for block:" + to_string(_header.getBlockId()) +
                  "  proposer index:" + to_string(_header.getProposerIndex()));
        responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT, CONNECTION_DOUBLE_PROPOSAL);
        responseHeader->setComplete();
        return responseHeader;
    }
    responseHeader->setStatus(CONNECTION_PROCEED);
    responseHeader->setComplete();
    return responseHeader;
}

ptr<Header> BlockProposalServerAgent::createDAProofResponseHeader(ptr<ServerConnection>
        _connectionEnvelope,
                                                                   DAProofRequestHeader
                                                                   _header) {

    auto responseHeader = make_shared<DAProofResponseHeader>();

    if (sChain->getSchainID() != _header.getSchainId()) {
        responseHeader->setStatusSubStatus(CONNECTION_SERVER_ERROR, CONNECTION_ERROR_UNKNOWN_SCHAIN_ID);
        BOOST_THROW_EXCEPTION(
                InvalidSchainException("Incorrect schain " + to_string(_header.getSchainId()), __CLASS_NAME__));
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

    ptr<SHAHash> blockHash = nullptr;
    try {
        blockHash = SHAHash::fromHex(_header.getBlockHash());
    } catch(...) {
        responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT, CONNECTION_INVALID_HASH);
        responseHeader->setComplete();
        return responseHeader;
    }


    ptr<ThresholdSignature> sig;

    try {

        sig = getSchain()->getCryptoManager()->verifyThreshold(blockHash,
                                                         _header.getSignature(), _header.getBlockId());

    } catch(...) {
        responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT, CONNECTION_SIGNATURE_DID_NOT_VERIFY);
        responseHeader->setComplete();
        return responseHeader;
    }


    auto proposal = getSchain()->getBlockProposal(_header.getBlockId(), _header.getProposerIndex());

    if (proposal == nullptr) {
        responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT, CONNECTION_DONT_HAVE_THIS_PROPOSAL);
        responseHeader->setComplete();
        return responseHeader;
    }

    if (*proposal->getHash()->toHex() != *_header.getBlockHash()) {
        responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT, CONNECTION_INVALID_HASH);
        responseHeader->setComplete();
        return responseHeader;
    }



    if (proposal->getDaProof() != nullptr) {
        responseHeader->setStatusSubStatus(CONNECTION_DISCONNECT, CONNECTION_ALREADY_HAVE_DAP_PROOF);
        responseHeader->setComplete();
        return responseHeader;
    }

    auto proof = make_shared<DAProof>(proposal, sig);

    sChain->daProofArrived(proof);

    responseHeader->setStatus(CONNECTION_SUCCESS);
    responseHeader->setComplete();
    return responseHeader;

}



ptr<Header> BlockProposalServerAgent::createFinalResponseHeader(ptr<ReceivedBlockProposal> _proposal) {

    auto sigShare = getSchain()->getCryptoManager()->signThreshold(_proposal->getHash(), _proposal->getBlockID());

    auto responseHeader = make_shared<FinalProposalResponseHeader>(sigShare->toString());

    responseHeader->setStatus(CONNECTION_SUCCESS);
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
                                            partialHashesList->getPartialHashes(),
                                            msg_len((uint64_t) partialHashesList->getTransactionCount() *
                                                    PARTIAL_SHA_HASH_LEN));
        } catch (ExitRequestedException &) { throw; }
        catch (...) {
            throw_with_nested(
                    CouldNotReadPartialDataHashesException("Could not read partial hashes", __CLASS_NAME__));
        }
    }

    return partialHashesList;

}
