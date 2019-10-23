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

    @file BlockProposalPusherAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../../Agent.h"
#include "../../Log.h"
#include "../../SkaleCommon.h"
#include "../../exceptions/FatalError.h"
#include "../../thirdparty/json.hpp"

#include "../../abstracttcpserver/ConnectionStatus.h"
#include "../../crypto/SHAHash.h"
#include "../../datastructures/PartialHashesList.h"
#include "../../datastructures/Transaction.h"
#include "../../datastructures/TransactionList.h"


#include "../../chains/Schain.h"
#include "../../crypto/CryptoManager.h"
#include "../../datastructures/BlockProposal.h"
#include "../../datastructures/CommittedBlock.h"
#include "../../exceptions/NetworkProtocolException.h"
#include "../../headers/BlockProposalHeader.h"
#include "../../headers/MissingTransactionsRequestHeader.h"
#include "../../headers/MissingTransactionsResponseHeader.h"
#include "../../headers/FinalProposalResponseHeader.h"
#include "../../network/ClientSocket.h"
#include "../../network/ServerConnection.h"
#include "../../network/IO.h"
#include "../../network/TransportNetwork.h"
#include "../../node/Node.h"
#include "../../pendingqueue/PendingTransactionsAgent.h"

#include "../../abstracttcpclient/AbstractClientAgent.h"
#include "../../exceptions/ExitRequestedException.h"
#include "../../exceptions/PingException.h"
#include "BlockProposalClientAgent.h"
#include "BlockProposalPusherThreadPool.h"


BlockProposalClientAgent::BlockProposalClientAgent(Schain &_sChain)
        : AbstractClientAgent(_sChain, PROPOSAL) {
    try {
        LOG(debug, "Constructing blockProposalPushAgent");

        this->blockProposalThreadPool = make_shared<BlockProposalPusherThreadPool>(
                num_threads((uint64_t) _sChain.getNodeCount()), this);
        blockProposalThreadPool->startService();
    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }
}


ptr<MissingTransactionsRequestHeader>
BlockProposalClientAgent::readAndProcessMissingTransactionsRequestHeader(
        ptr<ClientSocket> _socket) {
    auto js =
            sChain->getIo()->readJsonHeader(_socket->getDescriptor(), "Read missing trans request");
    auto mtrh = make_shared<MissingTransactionsRequestHeader>();

    auto status = (ConnectionStatus) Header::getUint64(js, "status");
    auto substatus = (ConnectionSubStatus) Header::getUint64(js, "substatus");
    mtrh->setStatusSubStatus(status, substatus);

    auto count = (uint64_t) Header::getUint64(js, "count");
    mtrh->setMissingTransactionsCount(count);

    mtrh->setComplete();
    LOG(trace, "Push agent processed missing transactions header");
    return mtrh;
}

ptr<FinalProposalResponseHeader>
BlockProposalClientAgent::readAndProcessFinalProposalResponseHeader(
        ptr<ClientSocket> _socket) {
    auto js =
            sChain->getIo()->readJsonHeader(_socket->getDescriptor(), "Read final response header");

    auto status = (ConnectionStatus) Header::getUint64(js, "status");

    if (status != CONNECTION_SUCCESS) {
        LOG(err, "Server refused block sig");
        return nullptr;
    }
    return make_shared<FinalProposalResponseHeader>(Header::getString(js, "sigShare"));
}


void BlockProposalClientAgent::sendItemImpl(
        ptr<DataStructure> _item, shared_ptr<ClientSocket> _socket, schain_index _index, node_id _nodeID) {

    ptr<BlockProposal> _proposal = dynamic_pointer_cast<BlockProposal>(_item);
    
    sendBlockProposal(_proposal, _socket, _index, _nodeID);
}


void BlockProposalClientAgent::sendBlockProposal(
        ptr<BlockProposal> _proposal, shared_ptr<ClientSocket> socket, schain_index _index, node_id _nodeID) {


    LOG(trace, "Proposal step 0: Starting block proposal");

    CHECK_ARGUMENT(_proposal != nullptr);



    assert(_proposal != nullptr);

    ptr<Header> header = BlockProposal::createBlockProposalHeader(sChain, _proposal);


    try {
        getSchain()->getIo()->writeHeader(socket, header);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(NetworkProtocolException("Could not write header", __CLASS_NAME__));
    }


    LOG(trace, "Proposal step 1: wrote proposal header");

    auto response =
            sChain->getIo()->readJsonHeader(socket->getDescriptor(), "Read proposal resp");


    LOG(trace, "Proposal step 2: read proposal response");


    auto status = (ConnectionStatus) Header::getUint64(response, "status");
    // auto substatus = (ConnectionSubStatus) Header::getUint64(response, "substatus");


    if (status != CONNECTION_PROCEED) {
        LOG(trace, "Proposal Server terminated proposal push");
        return;
    }

    auto partialHashesList = _proposal->createPartialHashesList();


    if (partialHashesList->getTransactionCount() > 0) {
        try {
            getSchain()->getIo()->writeBytesVector(
                    socket->getDescriptor(), partialHashesList->getPartialHashes());
        } catch (ExitRequestedException &) {
            throw;
        } catch (...) {
            auto errStr = "Unexpected disconnect writing block data";
            throw_with_nested(NetworkProtocolException(errStr, __CLASS_NAME__));
        }
    }


    LOG(trace, "Proposal step 3: sent partial hashes");

    ptr<MissingTransactionsRequestHeader> missingTransactionHeader;

    try {
        missingTransactionHeader = readAndProcessMissingTransactionsRequestHeader(socket);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        auto errStr = "Could not read missing transactions request header";
        throw_with_nested(NetworkProtocolException(errStr, __CLASS_NAME__));
    }

    auto count = missingTransactionHeader->getMissingTransactionsCount();

    if (count == 0) {
        LOG(trace, "Proposal complete::no missing transactions");
        return;
    }

    ptr<unordered_set<ptr<partial_sha_hash>, PendingTransactionsAgent::Hasher,
            PendingTransactionsAgent::Equal> >
            missingHashes;

    try {
        missingHashes = readMissingHashes(socket, count);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        auto errStr = "Could not read missing hashes";
        throw_with_nested(NetworkProtocolException(errStr, __CLASS_NAME__));
    }


    LOG(trace, "Proposal step 4: read missing transaction hashes");


    auto missingTransactions = make_shared<vector<ptr<Transaction> > >();
    auto missingTransactionsSizes = make_shared<vector<uint64_t> >();

    for (auto &&transaction : *_proposal->getTransactionList()->getItems()) {
        if (missingHashes->count(transaction->getPartialHash())) {
            missingTransactions->push_back(transaction);
            missingTransactionsSizes->push_back(transaction->getSerializedSize(false));
        }
    }

    ASSERT2(missingTransactions->size() == count,
            "Transactions:" + to_string(missingTransactions->size()) + ":" + to_string(count));


    auto mtrh = make_shared<MissingTransactionsResponseHeader>(missingTransactionsSizes);

    try {
        getSchain()->getIo()->writeHeader(socket, mtrh);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        auto errString =
                "Proposal: unexpected server disconnect writing missing txs response header";
        throw_with_nested(new NetworkProtocolException(errString, __CLASS_NAME__));
    }


    LOG(trace, "Proposal step 5: sent missing transactions header");


    auto mtrm = make_shared<TransactionList>(missingTransactions);

    try {
        getSchain()->getIo()->writeBytesVector(socket->getDescriptor(), mtrm->serialize(false));
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        auto errString = "Proposal: unexpected server disconnect  writing missing hashes";
        throw_with_nested(new NetworkProtocolException(errString, __CLASS_NAME__));
    }

    LOG(trace, "Proposal step 6: sent missing transactions");

    auto finalHeader = readAndProcessFinalProposalResponseHeader(socket);

    if (finalHeader == nullptr)
        return;

    auto sigShare = getSchain()->getCryptoManager()->createSigShare(finalHeader->getSigShare(),
                                                                    _proposal->getSchainID(),
                                                                    _proposal->getBlockID(), _nodeID, _index,
                                                                    getSchain()->getTotalSignersCount(),
                                                                    getSchain()->getRequiredSignersCount());

    LOG(err, "Sig share arrived");
    getSchain()->sigShareArrived(sigShare);

    LOG(trace, "Proposal step 7: got final response");
}


ptr<unordered_set<ptr<partial_sha_hash>, PendingTransactionsAgent::Hasher,
        PendingTransactionsAgent::Equal> >

BlockProposalClientAgent::readMissingHashes(ptr<ClientSocket> _socket, uint64_t _count) {
    ASSERT(_count);
    auto bytesToRead = _count * PARTIAL_SHA_HASH_LEN;
    vector<uint8_t> buffer(bytesToRead);

    ASSERT(bytesToRead > 0);


    try {
        getSchain()->getIo()->readBytes(
                _socket->getDescriptor(), (in_buffer *) buffer.data(), msg_len(bytesToRead));
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        LOG(info, "Could not read partial hashes");
        throw_with_nested(
                NetworkProtocolException("Could not read partial data hashes", __CLASS_NAME__));
    }


    auto result = make_shared<unordered_set<ptr<partial_sha_hash>,
            PendingTransactionsAgent::Hasher, PendingTransactionsAgent::Equal> >();


    try {
        for (uint64_t i = 0; i < _count; i++) {
            auto hash = make_shared<partial_sha_hash>();
            for (size_t j = 0; j < PARTIAL_SHA_HASH_LEN; j++) {
                hash->at(j) = buffer.at(PARTIAL_SHA_HASH_LEN * i + j);
            }

            result->insert(hash);
            ASSERT(result->count(hash));
        }
    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(NetworkProtocolException(
                "Could not read missing transaction hashes:" + to_string(_count), __CLASS_NAME__));
    }


    ASSERT(result->size() == _count);

    return result;
}
