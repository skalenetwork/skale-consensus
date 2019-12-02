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

    @file CommittedBlock.cpp
    @author Stan Kladko
    @date 2018
*/

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>


#include "../Log.h"
#include "../SkaleCommon.h"
#include "../thirdparty/json.hpp"
#include "../crypto/CryptoManager.h"
#include "../crypto/ThresholdSignature.h"

#include "../abstracttcpserver/ConnectionStatus.h"
#include "../chains/Schain.h"
#include "../crypto/SHAHash.h"
#include "../exceptions/NetworkProtocolException.h"
#include "../exceptions/ParsingException.h"
#include "../exceptions/InvalidStateException.h"
#include "../exceptions/ExitRequestedException.h"
#include "../headers/BlockProposalHeader.h"
#include "../headers/CommittedBlockHeader.h"


#include "../datastructures/Transaction.h"
#include "../network/Buffer.h"
#include "TransactionList.h"
#include "BlockProposalFragment.h"
#include "CommittedBlock.h"


CommittedBlock::CommittedBlock(ptr<BlockProposal> _p, ptr<ThresholdSignature> _thresholdSig)
        : BlockProposal(_p->getSchainID(), _p->getProposerNodeID(),
                        _p->getBlockID(), _p->getProposerIndex(), _p->getTransactionList(), _p->getTimeStamp(),
                        _p->getTimeStampMs(), ptr<string>(), ptr<CryptoManager>()) {
    CHECK_ARGUMENT(_thresholdSig != nullptr);
    CHECK_ARGUMENT(_p != nullptr);
    this->signature = _p->getSignature();
    this->thresholdSig = _thresholdSig->toString();
    CHECK_STATE(signature != nullptr);
}


void CommittedBlock::serializedSanityCheck(ptr<vector<uint8_t> > _serializedBlock) {
    CHECK_STATE(_serializedBlock->at(sizeof(uint64_t)) == '{');
    CHECK_STATE(_serializedBlock->back() == '>');
};




ptr<CommittedBlock> CommittedBlock::deserialize(ptr<vector<uint8_t> > _serializedBlock,
                                                ptr<CryptoManager> _manager) {

    ptr<string> headerStr = extractHeader(_serializedBlock);

    ptr<CommittedBlockHeader> blockHeader;

    try {
        blockHeader = CommittedBlock::parseBlockHeader(headerStr);
    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(ParsingException(
                "Could not parse committed block header: \n" + *headerStr, __CLASS_NAME__));
    }

    auto list = deserializeTransactions(blockHeader, headerStr, _serializedBlock);

    auto block = make_shared<CommittedBlock>(blockHeader->getSchainID(), blockHeader->getProposerNodeId(),
                                             blockHeader->getBlockID(), blockHeader->getProposerIndex(),
                                             list, blockHeader->getTimeStamp(), blockHeader->getTimeStampMs(),
                                             blockHeader->getSignature(),
                                             blockHeader->getThresholdSig());

    _manager->verifyProposalECDSA(block.get(), blockHeader->getBlockHash(), blockHeader->getSignature());

    return block;
}


ptr<CommittedBlockHeader> CommittedBlock::parseBlockHeader(const shared_ptr<string> &header) {
    CHECK_ARGUMENT(header != nullptr);
    CHECK_ARGUMENT(header->size() > 2);
    CHECK_ARGUMENT2(header->at(0) == '{', "Block header does not start with {");
    CHECK_ARGUMENT2(header->at(header->size() - 1) == '}', "Block header does not end with }");

    auto js = nlohmann::json::parse(*header);

    return make_shared<CommittedBlockHeader>(js);

}

CommittedBlock::CommittedBlock(uint64_t
                               timeStamp, uint32_t
                               timeStampMs)
        : BlockProposal(timeStamp, timeStampMs) {}


CommittedBlock::CommittedBlock(
        const schain_id &sChainId,
        const node_id &proposerNodeId,
        const block_id &blockId,
        const schain_index &proposerIndex,
        const ptr<TransactionList> &transactions,
        uint64_t timeStamp,
        __uint32_t timeStampMs, ptr<string>
        _signature, ptr<string> _thresholdSig)
        : BlockProposal(sChainId, proposerNodeId, blockId, proposerIndex, transactions, timeStamp,
                        timeStampMs, ptr<string>(), ptr<CryptoManager>()) {
    CHECK_ARGUMENT(_signature != nullptr);
    CHECK_ARGUMENT(_thresholdSig != nullptr);
    this->signature = _signature;
    this->thresholdSig = _thresholdSig;
}


ptr<CommittedBlock> CommittedBlock::createRandomSample(ptr<CryptoManager> _manager, uint64_t
_size,
                                                       boost::random::mt19937 &_gen,
                                                       boost::random::uniform_int_distribution<> &_ubyte,
                                                       block_id
                                                       _blockID) {
    auto list = TransactionList::createRandomSample(_size, _gen, _ubyte);

    static uint64_t MODERN_TIME = 1547640182;

    auto proposal = make_shared<BlockProposal>(1, 1, _blockID, 1, list, MODERN_TIME + 1, 1, nullptr,
            _manager);


    return make_shared<CommittedBlock>(proposal, nullptr);
}

ptr<BlockProposalFragment> CommittedBlock::getFragment(uint64_t _totalFragments, fragment_index _index) {

    CHECK_ARGUMENT(_totalFragments > 0);
    CHECK_ARGUMENT(_index <= _totalFragments);
    LOCK(m)

    auto sBlock = serialize();
    auto blockSize = sBlock->size();

    uint64_t fragmentStandardSize;

    if (blockSize % _totalFragments == 0) {
        fragmentStandardSize = sBlock->size() / _totalFragments;
    } else {
        fragmentStandardSize = sBlock->size() / _totalFragments + 1;
    }

    auto startIndex = fragmentStandardSize * ((uint64_t) _index - 1);


    auto fragmentData = make_shared<vector<uint8_t>>();
    fragmentData->reserve(fragmentStandardSize + 2);

    fragmentData->push_back('<');


    if (_index == _totalFragments) {
        fragmentData->insert(fragmentData->begin() + 1, sBlock->begin() + startIndex,
                             sBlock->end());

    } else {
        fragmentData->insert(fragmentData->begin() + 1, sBlock->begin() + startIndex,
                             sBlock->begin() + startIndex + fragmentStandardSize);
    }

    fragmentData->push_back('>');


    return make_shared<BlockProposalFragment>(getBlockID(), _totalFragments, _index, fragmentData,
                                              sBlock->size(), getHash()->toHex());
}

ptr<Header> CommittedBlock::createHeader() {
    return make_shared<BlockProposalHeader>(*this);
}
