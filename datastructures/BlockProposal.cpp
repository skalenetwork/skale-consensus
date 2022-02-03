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

    @file BlockProposal.cpp
    @author Stan Kladko
    @date 2018
*/


#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>


#include "SkaleCommon.h"
#include "Log.h"

#include <network/Utils.h>
#include "exceptions/FatalError.h"
#include "exceptions/InvalidArgumentException.h"
#include "exceptions/ParsingException.h"
#include "crypto/BLAKE3Hash.h"
#include "crypto/CryptoManager.h"
#include "crypto/EncryptedArgument.h"
#include "datastructures/BlockEncryptedArguments.h"
#include "network/Buffer.h"
#include "node/ConsensusEngine.h"
#include "exceptions/ExitRequestedException.h"
#include "headers/BlockProposalHeader.h"
#include "chains/Schain.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "datastructures/BlockProposalFragment.h"
#include "datastructures/BlockProposalFragmentList.h"
#include "headers/BlockProposalRequestHeader.h"

#include "utils//Time.h"

#include "Transaction.h"
#include "TransactionList.h"
#include "PartialHashesList.h"
#include "BlockProposal.h"


using namespace std;

BLAKE3Hash BlockProposal::getHash() {
    return hash;
}


void BlockProposal::calculateHash() {

    HASH_INIT(hasher);

    HASH_UPDATE(hasher, proposerIndex);
    HASH_UPDATE(hasher, proposerNodeID);
    HASH_UPDATE(hasher, schainID);
    HASH_UPDATE(hasher, blockID);
    HASH_UPDATE(hasher, transactionCount);
    HASH_UPDATE(hasher, timeStamp);
    HASH_UPDATE(hasher, timeStampMs);

    uint32_t sz = transactionList->size();

    HASH_UPDATE(hasher, sz);

    // export into 8-bit unsigned values, most significant bit first:
    auto sr = Utils::u256ToBigEndianArray(getStateRoot());
    auto v = Utils::carray2Hex(sr->data(), sr->size());
    blake3_hasher_update(&hasher, (unsigned char *) v.data(), v.size());

    if (transactionList->size() > 0) {
        auto merkleRoot = transactionList->calculateTopMerkleRoot();
        blake3_hasher_update(&hasher, merkleRoot.getHash().data(), HASH_LEN);
    }
    auto buf = make_shared<array<uint8_t, HASH_LEN>>();

    blake3_hasher_finalize(&hasher, hash.data(), BLAKE3_OUT_LEN);

};


BlockProposal::BlockProposal(uint64_t _timeStamp, uint32_t _timeStampMs) : timeStamp(_timeStamp),
                                                                           timeStampMs(_timeStampMs) {
    proposerNodeID = 0;
    creationTime = Time::getCurrentTimeMs();
};

BlockProposal::BlockProposal(schain_id _sChainId, node_id _proposerNodeId, block_id _blockID,
                             schain_index _proposerIndex, const ptr<TransactionList> &_transactions, u256 _stateRoot,
                             uint64_t _timeStamp, __uint32_t _timeStampMs, const string &_signature,
                             const ptr<CryptoManager> &_cryptoManager)
        : schainID(_sChainId), proposerNodeID(_proposerNodeId), blockID(_blockID),
          proposerIndex(_proposerIndex), timeStamp(_timeStamp), timeStampMs(_timeStampMs),
          stateRoot(_stateRoot), transactionList(_transactions), signature(_signature) {
    creationTime = Time::getCurrentTimeMs();
    CHECK_ARGUMENT(_transactions);

    if (_proposerIndex == 0) {
        stateRoot = 0;
    }


    CHECK_STATE(timeStamp > MODERN_TIME);

    transactionCount = transactionList->getItems()->size();
    calculateHash();

    if (_cryptoManager != nullptr) {
        _cryptoManager->signProposal(this);
    } else {
        CHECK_ARGUMENT(_signature != "");
        signature = _signature;
    }
}


ptr<PartialHashesList> BlockProposal::createPartialHashesList() {

    auto s = (uint64_t) this->transactionCount * PARTIAL_HASH_LEN;

    CHECK_STATE(transactionList);

    auto t = transactionList->getItems();

    if (s > MAX_BUFFER_SIZE) {
        InvalidArgumentException("Buffer size too large", __CLASS_NAME__);
    }

    auto partialHashes = make_shared<vector<uint8_t>>(s);

    for (uint64_t i = 0; i < transactionCount; i++) {

        for (size_t j = 0; j < PARTIAL_HASH_LEN; j++) {
            partialHashes->at(i * PARTIAL_HASH_LEN + j) = t->at(i)->getHash().at(j);
        }
    }

    return make_shared<PartialHashesList>((transaction_count) transactionCount, partialHashes);

}

BlockProposal::~BlockProposal() {

}

block_id BlockProposal::getBlockID() const {
    return blockID;
}


schain_index BlockProposal::getProposerIndex() const {
    return proposerIndex;
}


node_id BlockProposal::getProposerNodeID() const {
    return proposerNodeID;
}


ptr<TransactionList> BlockProposal::getTransactionList() {
    CHECK_STATE(transactionList);
    return transactionList;
}

schain_id BlockProposal::getSchainID() const {
    return schainID;
}

transaction_count BlockProposal::getTransactionCount() const {
    return transactionCount;
}

uint64_t BlockProposal::getTimeStampS() const {
    return timeStamp;
}


uint32_t BlockProposal::getTimeStampMs() const {
    return timeStampMs;
}

void BlockProposal::addSignature(const string &_signature) {
    CHECK_ARGUMENT(!_signature.empty())
    signature = _signature;
}

string BlockProposal::getSignature() {
    CHECK_STATE(!signature.empty())
    return signature;
}

ptr<BlockProposalRequestHeader> BlockProposal::createBlockProposalHeader(Schain *_sChain,
                                                                         const ptr<BlockProposal> &_proposal) {

    CHECK_ARGUMENT(_sChain);
    CHECK_ARGUMENT(_proposal);

    LOCK(_proposal->m);

    if (_proposal->header)
        return _proposal->header;

    _proposal->header = make_shared<BlockProposalRequestHeader>(*_sChain, _proposal);

    return _proposal->header;

}


ptr<BasicHeader> BlockProposal::createHeader(uint64_t) {
    return make_shared<BlockProposalHeader>(*this);
}

ptr<vector<uint8_t> > BlockProposal::serialize(uint64_t _flags) {

    LOCK(m)

    if (serializedProposal != nullptr)
        return serializedProposal;

    auto blockHeader = createHeader(_flags);

    auto buf = blockHeader->toBuffer();

    CHECK_STATE(buf);
    CHECK_STATE(buf->getBuf()->at(sizeof(uint64_t)) == '{');
    CHECK_STATE(buf->getBuf()->at(buf->getCounter() - 1) == '}');

    auto block = make_shared<vector<uint8_t> >();

    block->insert(
            block->end(), buf->getBuf()->begin(), buf->getBuf()->begin() + buf->getCounter());

    CHECK_STATE(transactionList);

    auto serializedList = transactionList->serialize(true);

    CHECK_STATE(serializedList);

    CHECK_STATE(serializedList->front() == '<');
    CHECK_STATE(serializedList->back() == '>');


    block->insert(block->end(), serializedList->begin(), serializedList->end());

    if (transactionList->size() == 0) {
        CHECK_STATE(block->size() == buf->getCounter() + 2);
    }

    CHECK_STATE(block);

    serializedProposal = block;

    CHECK_STATE(block->at(sizeof(uint64_t)) == '{');
    CHECK_STATE(block->back() == '>');

    return block;
}


ptr<BlockProposal> BlockProposal::deserialize(const ptr<vector<uint8_t> > &_serializedProposal,
                                              const ptr<CryptoManager> &_manager) {

    CHECK_ARGUMENT(_serializedProposal);
    CHECK_ARGUMENT(_manager);

    string headerStr = BlockProposal::extractHeader(_serializedProposal);

    CHECK_STATE(!headerStr.empty());

    ptr<BlockProposalHeader> blockHeader;

    try {
        blockHeader = parseBlockHeader(headerStr);
        CHECK_STATE(blockHeader);
    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(ParsingException(
                "Could not parse block header: \n" + headerStr, __CLASS_NAME__));
    }

    auto list = deserializeTransactions(blockHeader, headerStr, _serializedProposal);

    CHECK_STATE(list);

    auto sig = blockHeader->getSignature();

    CHECK_STATE(!sig.empty());

    auto proposal = make_shared<BlockProposal>(blockHeader->getSchainID(), blockHeader->getProposerNodeId(),
                                               blockHeader->getBlockID(), blockHeader->getProposerIndex(),
                                               list, blockHeader->getStateRoot(), blockHeader->getTimeStamp(),
                                               blockHeader->getTimeStampMs(),
                                               blockHeader->getSignature(), nullptr);

    _manager->verifyProposalECDSA(proposal, blockHeader->getBlockHash(), blockHeader->getSignature());

    proposal->serializedProposal = _serializedProposal;

    return proposal;
}

ptr<BlockProposal>
BlockProposal::defragment(const ptr<BlockProposalFragmentList> &_fragmentList,
                          const ptr<CryptoManager> &_cryptoManager) {

    CHECK_ARGUMENT(_fragmentList);
    CHECK_ARGUMENT(_cryptoManager);

    try {
        auto result = deserialize(_fragmentList->serialize(), _cryptoManager);
        CHECK_STATE(result);
        return result;
    } catch (exception &e) {
        SkaleException::logNested(e);
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

ptr<BlockProposalFragment> BlockProposal::getFragment(uint64_t _totalFragments, fragment_index _index) {

    CHECK_ARGUMENT(_totalFragments > 0);
    CHECK_ARGUMENT(_index <= _totalFragments);
    LOCK(m)

    auto serializedBlock = serialize(SERIALIZE_AS_PROPOSAL);

    CHECK_STATE(serializedBlock);

    auto blockSize = serializedBlock->size();

    uint64_t fragmentStandardSize;

    if (blockSize % _totalFragments == 0) {
        fragmentStandardSize = serializedBlock->size() / _totalFragments;
    } else {
        fragmentStandardSize = serializedBlock->size() / _totalFragments + 1;
    }

    auto startIndex = fragmentStandardSize * ((uint64_t) _index - 1);

    auto fragmentData = make_shared<vector<uint8_t>>();

    fragmentData->reserve(fragmentStandardSize + 2);

    fragmentData->push_back('<');

    if (_index == _totalFragments) {
        fragmentData->insert(fragmentData->begin() + 1, serializedBlock->begin() + startIndex,
                             serializedBlock->end());
    } else {
        fragmentData->insert(fragmentData->begin() + 1, serializedBlock->begin() + startIndex,
                             serializedBlock->begin() + startIndex + fragmentStandardSize);
    }

    fragmentData->push_back('>');

    return make_shared<BlockProposalFragment>(getBlockID(), _totalFragments, _index, fragmentData,
                                              serializedBlock->size(), getHash().toHex());
}

ptr<TransactionList> BlockProposal::deserializeTransactions(const ptr<BlockProposalHeader> &_header,
                                                            const string &_headerString,
                                                            const ptr<vector<uint8_t> > &_serializedBlock) {

    CHECK_ARGUMENT(_header);
    CHECK_ARGUMENT(_headerString != "");
    CHECK_ARGUMENT(_serializedBlock);

    auto headerSize = _headerString.size();

    ptr<TransactionList> list;
    try {
        list = TransactionList::deserialize(
                _header->getTransactionSizes(), _serializedBlock, headerSize + sizeof(headerSize), true);
        CHECK_STATE(list);

    } catch (...) {
        throw_with_nested(
                ParsingException("Could not parse transactions after header. Header: \n" + _headerString +
                                 " Transactions size:" + to_string(_serializedBlock->size()),
                                 __CLASS_NAME__)
        );
    }

    return list;

}


string BlockProposal::extractHeader(const ptr<vector<uint8_t> > &_serializedBlock) {

    try {

        CHECK_ARGUMENT(_serializedBlock);

        uint64_t headerSize = 0;

        auto size = _serializedBlock->size();

        CHECK_ARGUMENT2(
                size >= sizeof(headerSize) + 2, "Serialized block too small:" + to_string(size));

        using boost::iostreams::array_source;
        using boost::iostreams::stream;

        array_source src((char *) _serializedBlock->data(), _serializedBlock->size());

        stream<array_source> in(src);

        in.read((char *) &headerSize, sizeof(headerSize)); /* Flawfinder: ignore */

        CHECK_STATE2(headerSize >= 2 && headerSize + sizeof(headerSize) <= _serializedBlock->size(),
                     "Invalid header size" + to_string(headerSize));


        CHECK_STATE(headerSize <= MAX_BUFFER_SIZE);

        CHECK_STATE(_serializedBlock->at(headerSize + sizeof(headerSize)) == '<');
        CHECK_STATE(_serializedBlock->at(sizeof(headerSize)) == '{');
        CHECK_STATE(_serializedBlock->back() == '>');

        string header(headerSize, ' ');

        in.read((char *) header.c_str(), headerSize); /* Flawfinder: ignore */

        return header;

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


ptr<BlockProposalHeader> BlockProposal::parseBlockHeader(const string &_header) {
    try {
        CHECK_ARGUMENT(_header != "");
        CHECK_ARGUMENT(_header.size() > 2);
        CHECK_ARGUMENT2(_header.at(0) == '{', "Block header does not start with {");
        CHECK_ARGUMENT2(
                _header.at(_header.size() - 1) == '}', "Block header does not end with }");

        auto js = nlohmann::json::parse(_header);

        return make_shared<BlockProposalHeader>(js);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

u256 BlockProposal::getStateRoot() const {
    return stateRoot;
}

TimeStamp BlockProposal::getTimeStamp() const {
    return TimeStamp(getTimeStampS(), getTimeStampMs());
}


uint64_t BlockProposal::getCreationTime() const {
    return creationTime;
}

ptr<BlockEncryptedArguments> BlockProposal::getEncryptedArguments(Schain &_schain) {
    try {
        LOCK(cachedEncryptedArgumentsLock);
        if (cachedEncryptedArguments) {
            return cachedEncryptedArguments;
        }

        cachedEncryptedArguments = make_shared<BlockEncryptedArguments>();

        if (!transactionList) {
            return cachedEncryptedArguments;
        }

        auto transactions = transactionList->getItems();

        for (uint64_t i = 0; i < transactions->size(); i++) {
            auto rawArg = _schain.getExtFace()->getEncryptedData( *transactions->at(i)->getData() );
            if (rawArg) {
                auto argument = make_shared<EncryptedArgument>(rawArg);
                cachedEncryptedArguments->insert(i, argument);
            }
        }

        return cachedEncryptedArguments;

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}
