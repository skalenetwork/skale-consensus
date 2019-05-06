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

    @file CommittedBlock.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"


#include "../thirdparty/json.hpp"
#include "../crypto/SHAHash.h"
#include "../abstracttcpserver/ConnectionStatus.h"
#include "../headers/CommittedBlockHeader.h"
#include "../exceptions/NetworkProtocolException.h"
#include "../exceptions/ParsingException.h"
#include "../exceptions/InvalidArgumentException.h"


#include "../datastructures/Transaction.h"
#include "TransactionList.h"
#include "../network/Buffer.h"
#include "CommittedBlock.h"

CommittedBlock::CommittedBlock(Schain &_sChain, ptr<BlockProposal> _p) : BlockProposal(_sChain,
                                                                                       _p->getBlockID(),
                                                                                       _p->getProposerIndex(),
                                                                                       _p->getTransactionList(),
                                                                                       _p->getTimeStamp()) {
}


ptr<vector<uint8_t>> CommittedBlock::serialize() {


    if (serializedBlock != nullptr) {
        return serializedBlock;
    }

    auto items = transactionList->getItems();

    auto header = make_shared<CommittedBlockHeader>(*this);

    auto buf = header->toBuffer();

    ASSERT((*buf->getBuf())[sizeof(uint64_t)] == '{');
    ASSERT((*buf->getBuf())[buf->getCounter() - 1] == '}');


    uint64_t binSize = 0;

    for (auto &&tx: *items) {
        binSize += tx->getData()->size();
    }

    auto block = make_shared<vector<uint8_t>>();

    block->insert(block->end(), buf->getBuf()->begin(), buf->getBuf()->begin() + buf->getCounter());

    for (auto &&tx: *items) {
        auto data = tx->getData();
        block->insert(block->end(), data->begin(), data->end());
    }

    ASSERT((*block)[sizeof(uint64_t)] == '{');

    serializedBlock = block;

    return serializedBlock;
}

uint64_t CommittedBlock::getHeaderSize() const {
    return headerSize;
}

CommittedBlock::CommittedBlock(ptr<vector<uint8_t>> _serializedBlock) : BlockProposal(0) {

    auto size = _serializedBlock->size();

    if (size < sizeof(headerSize) + 2) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Serialized block size too small:" + to_string(size),
                                                       __CLASS_NAME__));
    }

    serializedBlock = _serializedBlock;

    std::memcpy(&headerSize, _serializedBlock->data(), sizeof(headerSize));


    if (headerSize < 3 || headerSize > _serializedBlock->size()) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Invalid header size" + to_string(headerSize), __CLASS_NAME__));
    }

    if ((*_serializedBlock)[sizeof(uint64_t)] != '{') {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Block does header does not start with {", __CLASS_NAME__));
    }

    if ((*_serializedBlock)[headerSize + sizeof(uint64_t) - 1] != '}') {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Block header does not end with }", __CLASS_NAME__));
    }

    if (headerSize > MAX_BUFFER_SIZE) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Header size too large", __CLASS_NAME__));
    }

    auto s = make_shared<string>((const char *) _serializedBlock->data() + sizeof(headerSize),
                                 headerSize);


    nlohmann::json js;
    auto transactionSizes = make_shared<vector<size_t>>();

    size_t totalSize = 0;


    try {
        js = nlohmann::json::parse(*s);

        this->proposerIndex = schain_index(Header::getUint64(js, "proposerIndex"));
        this->proposerNodeID = node_id(Header::getUint64(js, "proposerNodeID"));
        this->blockID = block_id(Header::getUint64(js, "blockID"));
        this->schainID = schain_id(Header::getUint64(js, "schainID"));
        this->timeStamp = Header::getUint64(js, "timeStamp");

        this->transactionCount = js["sizes"].size();
        this->hash = SHAHash::fromHex(Header::getString(js, "hash"));


        Header::nullCheck(js, "sizes");
        nlohmann::json jsonTransactionSizes = js["sizes"];
        this->transactionCount = jsonTransactionSizes.size();

        for (auto &&jsize : jsonTransactionSizes) {
            transactionSizes->push_back(jsize);
            totalSize += (size_t) jsize;
        }

    } catch (...) {
        throw_with_nested(ParsingException("Could not parse catchup block header: \n" + *s, __CLASS_NAME__));
    }

    transactionList = make_shared<TransactionList>(transactionSizes, _serializedBlock, headerSize);

    calculateHash();

};
