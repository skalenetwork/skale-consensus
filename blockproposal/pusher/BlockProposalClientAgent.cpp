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

#include "Agent.h"
#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"

#include "abstracttcpserver/ConnectionStatus.h"
#include "crypto/SHAHash.h"
#include "datastructures/PartialHashesList.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"


#include "chains/Schain.h"
#include "crypto/CryptoManager.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/CommittedBlock.h"
#include "datastructures/DAProof.h"
#include "exceptions/NetworkProtocolException.h"
#include "headers/BlockProposalRequestHeader.h"
#include "headers/FinalProposalResponseHeader.h"
#include "headers/MissingTransactionsRequestHeader.h"
#include "headers/MissingTransactionsResponseHeader.h"
#include "headers/SubmitDAProofRequestHeader.h"
#include "network/ClientSocket.h"
#include "network/IO.h"
#include "network/Network.h"
#include "network/ServerConnection.h"
#include "node/Node.h"
#include "pendingqueue/PendingTransactionsAgent.h"

#include "abstracttcpclient/AbstractClientAgent.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/PingException.h"
#include "BlockProposalClientAgent.h"
#include "BlockProposalPusherThreadPool.h"


BlockProposalClientAgent::BlockProposalClientAgent(Schain &_sChain)
        : AbstractClientAgent(_sChain, PROPOSAL) {

    sentProposals = make_shared<cache::lru_cache<uint64_t,
            ptr<list<pair<ConnectionStatus, ConnectionSubStatus>>>>>(32);

    try {
        LOG(debug, "Constructing blockProposalPushAgent");

        this->blockProposalThreadPool = make_shared<BlockProposalPusherThreadPool>(
                num_threads((uint64_t) _sChain.getNodeCount()), this);
        blockProposalThreadPool->startService();
    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }
}


ptr<MissingTransactionsRequestHeader> BlockProposalClientAgent::readMissingTransactionsRequestHeader(
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

ptr<FinalProposalResponseHeader> BlockProposalClientAgent::readAndProcessFinalProposalResponseHeader(
        ptr<ClientSocket> _socket) {
    auto js =
            sChain->getIo()->readJsonHeader(_socket->getDescriptor(), "Read final response header");

    auto status = (ConnectionStatus) Header::getUint64(js, "status");
    auto subStatus = (ConnectionSubStatus) Header::getUint64(js, "substatus");

    if (status == CONNECTION_SUCCESS) {
        return make_shared<FinalProposalResponseHeader>(Header::getString(js, "sigShare"));
    } else {
        LOG(err, "Proposal push failed:" + to_string(status) + ":" + to_string(subStatus));
        return make_shared<FinalProposalResponseHeader>(status, subStatus);
    }
}


pair<ConnectionStatus, ConnectionSubStatus>
BlockProposalClientAgent::sendItemImpl(ptr<DataStructure> _item, shared_ptr<ClientSocket> _socket,
                                       schain_index _index) {

    CHECK_ARGUMENT(_item);
    CHECK_ARGUMENT(_socket);

    ptr<BlockProposal> _proposal = dynamic_pointer_cast<BlockProposal>(_item);

    if (_proposal != nullptr) {

        pair<ConnectionStatus,
                ConnectionSubStatus> result = {ConnectionStatus::CONNECTION_STATUS_UNKNOWN,
                                               ConnectionSubStatus::CONNECTION_SUBSTATUS_UNKNOWN};

        auto key = (uint64_t) _index +
                   1024 * 1024 * (uint64_t) _proposal->getBlockID();

        if (!sentProposals->exists(key)) {
            sentProposals->put(key,
                               make_shared<list<pair<ConnectionStatus, ConnectionSubStatus>>>());
        }

        try {
            result = sendBlockProposal(_proposal, _socket, _index);
        } catch (...) {
            auto list = sentProposals->get(key);
            list->push_back(result);
            throw;
        }

        return result;
    }

    ptr<DAProof> _daProof = dynamic_pointer_cast<DAProof>(_item);

    if (_daProof != nullptr) {

        auto key = (uint64_t) _index +
                   1024 * 1024 * (uint64_t) _daProof->getBlockId();

        if (!sentProposals->exists(key)) {
            LOG(err, "Sending proof before proposal is sent");
        } else if (sentProposals->get(key)->back().first != CONNECTION_SUCCESS) {
            LOG(err, "Sending proof after failed proposal send: " +
                     to_string(sentProposals->get(key)->back().first) + ":" +
                     to_string(sentProposals->get(key)->back().second));
        }

        auto status = sendDAProof(_daProof, _socket);
        return status;
    }

    ASSERT(false);

}

ptr<BlockProposal> BlockProposalClientAgent::corruptProposal(ptr<BlockProposal> _proposal, schain_index _index) {
    if ((uint64_t) _index % 2 == 0) {
        auto proposal2 = make_shared<BlockProposal>(
                _proposal->getSchainID(), _proposal->getProposerNodeID(),
                _proposal->getBlockID(), _proposal->getProposerIndex(), make_shared<TransactionList>(
                        make_shared<vector<ptr<Transaction>>>()), _proposal->getStateRoot(), MODERN_TIME + 1, 1,
                nullptr, getSchain()->getCryptoManager());


        return proposal2;
    } else {
        return _proposal;
    }
}


pair<ConnectionStatus, ConnectionSubStatus>
BlockProposalClientAgent::sendBlockProposal(ptr<BlockProposal> _proposal, shared_ptr<ClientSocket> _socket,
                                            schain_index _index) {

    CHECK_ARGUMENT(_proposal);
    CHECK_ARGUMENT( _socket );


    INJECT_TEST(CORRUPT_PROPOSAL_TEST,
                _proposal = corruptProposal(_proposal, _index))

    LOG(trace, "Proposal step 0: Starting block proposal");


    ptr<Header> header = BlockProposal::createBlockProposalHeader(sChain, _proposal);

    CHECK_STATE(header);

    try {
        getSchain()->getIo()->writeHeader( _socket, header);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(NetworkProtocolException("Could not write header", __CLASS_NAME__));
    }


    LOG(trace, "Proposal step 1: wrote proposal header");

    auto response =
            sChain->getIo()->readJsonHeader( _socket->getDescriptor(), "Read proposal resp");


    LOG(trace, "Proposal step 2: read proposal response");

    pair<ConnectionStatus, ConnectionSubStatus> result =
            {ConnectionStatus::CONNECTION_STATUS_UNKNOWN,
             ConnectionSubStatus::CONNECTION_SUBSTATUS_UNKNOWN};


    try {

        result.first = (ConnectionStatus) Header::getUint64(response, "status");
        result.second = (ConnectionSubStatus) Header::getUint64(response, "substatus");
    } catch (...) {
    }


    if (result.first != CONNECTION_PROCEED) {
        LOG(trace, "Proposal Server terminated proposal push");
        return result;
    }

    auto partialHashesList = _proposal->createPartialHashesList();

    CHECK_STATE(partialHashesList);

    if (partialHashesList->getTransactionCount() > 0) {
        try {
            getSchain()->getIo()->writeBytesVector(
                _socket->getDescriptor(), partialHashesList->getPartialHashes());
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
        missingTransactionHeader = readMissingTransactionsRequestHeader( _socket );
        CHECK_STATE(missingTransactionHeader);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        auto errStr = "Could not read missing transactions request header";
        throw_with_nested(NetworkProtocolException(errStr, __CLASS_NAME__));
    }

    auto count = missingTransactionHeader->getMissingTransactionsCount();

    if (count == 0) {
        LOG(trace, "Proposal complete::no missing transactions");

    } else {

        ptr<unordered_set<ptr<partial_sha_hash>, PendingTransactionsAgent::Hasher,
                PendingTransactionsAgent::Equal> >
                missingHashes;

        try {
            missingHashes = readMissingHashes( _socket, count);
            CHECK_STATE(missingHashes);
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
            getSchain()->getIo()->writeHeader( _socket, mtrh);
        } catch (ExitRequestedException &) {
            throw;
        } catch (...) {
            auto errString =
                    "Proposal: unexpected server disconnect writing missing txs response header";
            throw_with_nested(new NetworkProtocolException(errString, __CLASS_NAME__));
        }


        LOG(trace, "Proposal step 5: sent missing transactions header");


        auto missingTransactionsList = make_shared<TransactionList>(missingTransactions);

        try {
            getSchain()->getIo()->writeBytesVector(
                _socket->getDescriptor(), missingTransactionsList->serialize(false));
        } catch (ExitRequestedException &) {
            throw;
        } catch (...) {
            auto errString = "Proposal: unexpected server disconnect  writing missing hashes";
            throw_with_nested(new NetworkProtocolException(errString, __CLASS_NAME__));
        }

        LOG(trace, "Proposal step 6: sent missing transactions");
    }

    auto finalHeader = readAndProcessFinalProposalResponseHeader( _socket );

    CHECK_STATE(finalHeader);


    pair<ConnectionStatus, ConnectionSubStatus> finalResult = {
            ConnectionStatus::CONNECTION_STATUS_UNKNOWN,
            ConnectionSubStatus::CONNECTION_SUBSTATUS_UNKNOWN
    };

    try {
        finalResult = finalHeader->getStatusSubStatus();
    } catch (...) {}

    if (finalResult.first != ConnectionStatus::CONNECTION_SUCCESS)
        return finalResult;

    auto sigShare = getSchain()->getCryptoManager()->createSigShare(finalHeader->getSigShare(),
                                                                    _proposal->getSchainID(),
                                                                    _proposal->getBlockID(), _index);
    CHECK_STATE(sigShare);

    getSchain()->daProofSigShareArrived(sigShare, _proposal);

    return finalResult;
}


pair<ConnectionStatus, ConnectionSubStatus> BlockProposalClientAgent::sendDAProof(
        ptr<DAProof> _daProof, ptr<ClientSocket> _socket) {


    CHECK_ARGUMENT(_daProof);
    CHECK_ARGUMENT(_socket);


    LOG(trace, "Proposal step 0: Starting block proposal");

    auto header = make_shared<SubmitDAProofRequestHeader>(*getSchain(), _daProof);

    try {
        getSchain()->getIo()->writeHeader(_socket, header);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(NetworkProtocolException("Could not write header", __CLASS_NAME__));
    }


    LOG(trace, "DA proof step 1: wrote request header");

    auto response =
            sChain->getIo()->readJsonHeader(_socket->getDescriptor(), "Read proposal resp");


    LOG(trace, "DAProof step 2: read response");

    ConnectionStatus status = ConnectionStatus::CONNECTION_STATUS_UNKNOWN;
    ConnectionSubStatus substatus = ConnectionSubStatus::CONNECTION_SUBSTATUS_UNKNOWN;

    status = (ConnectionStatus) Header::getUint64(response, "status");
    substatus = (ConnectionSubStatus) Header::getUint64(response, "substatus");


    if (status != CONNECTION_SUCCESS) {

        if (status == CONNECTION_RETRY_LATER)
            return {status, substatus};

        try {
            substatus = (ConnectionSubStatus) Header::getUint64(response, "substatus");
        } catch (...) {

        }
        if (substatus == CONNECTION_BLOCK_PROPOSAL_TOO_LATE) {
            LOG(trace, "Block proposal too late");
        } else {
            LOG(err, "Failure submitting DA proof:" + to_string(status) + ":" + to_string(substatus));
        }

        return {status, substatus};
    }


    return {status, substatus};
}


ptr<unordered_set<ptr<partial_sha_hash>, PendingTransactionsAgent::Hasher,
        PendingTransactionsAgent::Equal> >

BlockProposalClientAgent::readMissingHashes(ptr<ClientSocket> _socket, uint64_t _count) {
    CHECK_ARGUMENT(_socket);
    CHECK_ARGUMENT(_count > 0);

    auto bytesToRead = _count * PARTIAL_SHA_HASH_LEN;
    auto buffer = make_shared<vector<uint8_t>>(bytesToRead);

    CHECK_STATE(bytesToRead > 0);


    try {
        getSchain()->getIo()->readBytes(
                _socket->getDescriptor(), buffer, msg_len(bytesToRead));
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
                hash->at(j) = buffer->at(PARTIAL_SHA_HASH_LEN * i + j);
            }

            result->insert(hash);
        }
    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(NetworkProtocolException(
                "Could not read missing transaction hashes:" + to_string(_count), __CLASS_NAME__));
    }

    return result;
}
