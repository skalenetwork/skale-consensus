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
#include "Log.h"
#include "SkaleCommon.h"
#include "crypto/CryptoManager.h"
#include "crypto/ThresholdSignature.h"
#include "thirdparty/json.hpp"
#include "chains/Schain.h"
#include "crypto/BLAKE3Hash.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/InvalidStateException.h"
#include "exceptions/ParsingException.h"
#include "headers/BlockProposalHeader.h"
#include "headers/CommittedBlockHeader.h"


#include "BlockProposalFragment.h"
#include "CommittedBlock.h"
#include "TransactionList.h"
#include "datastructures/Transaction.h"
#include "network/Buffer.h"
#include "exceptions/InvalidSignatureException.h"
#include "exceptions/NetworkProtocolException.h"


ptr<CommittedBlock>
CommittedBlock::makeFromProposal(const ptr<BlockProposal> &_proposal, const ptr<ThresholdSignature> &_thresholdSig,
                                 ptr<ThresholdSignature> _daSig) {
    CHECK_ARGUMENT(_proposal);
    CHECK_ARGUMENT(_thresholdSig);
    CHECK_ARGUMENT(_daSig || _proposal->getProposerIndex() == 0)

    string daSig = "";

    if (_proposal->getProposerIndex() > 0) {
        daSig = _daSig->toString();
    }
    return CommittedBlock::make(_proposal->getSchainID(), _proposal->getProposerNodeID(),
                                _proposal->getBlockID(), _proposal->getProposerIndex(), _proposal->getTransactionList(),
                                _proposal->getStateRoot(), _proposal->getTimeStampS(), _proposal->getTimeStampMs(),
                                _proposal->getSignature(), _thresholdSig->toString(), daSig);
}

ptr<CommittedBlock> CommittedBlock::make(const schain_id _sChainId,
                                         const node_id _proposerNodeId, const block_id _blockId,
                                         schain_index _proposerIndex,
                                         const ptr<TransactionList> &_transactions, const u256 &_stateRoot,
                                         uint64_t _timeStamp,
                                         uint64_t _timeStampMs, const string &_signature, const string &_thresholdSig,
                                         const string &_daSig) {
    CHECK_ARGUMENT(_transactions);
    CHECK_ARGUMENT(!_signature.empty());
    CHECK_ARGUMENT(!_thresholdSig.empty());

    return make_shared<CommittedBlock>(_sChainId, _proposerNodeId, _blockId, _proposerIndex,
                                       _transactions, _stateRoot, _timeStamp, _timeStampMs, _signature, _thresholdSig,
                                       _daSig);
}


void CommittedBlock::serializedSanityCheck(const ptr<vector<uint8_t>> &_serializedBlock) {
    CHECK_ARGUMENT(_serializedBlock);
    CHECK_ARGUMENT(_serializedBlock->at(sizeof(uint64_t)) == '{');
    CHECK_ARGUMENT(_serializedBlock->back() == '>');
};


ptr<CommittedBlock> CommittedBlock::createRandomSample(const ptr<CryptoManager> &_manager,
                                                       uint64_t _size, boost::random::mt19937 &_gen,
                                                       boost::random::uniform_int_distribution<> &_ubyte,
                                                       block_id _blockID) {
    auto list = TransactionList::createRandomSample(_size, _gen, _ubyte);

    static uint64_t MODERN_TIME = 1547640182;


    u256 stateRoot = (uint64_t) _blockID + 1;


    auto p = make_shared<BlockProposal>(
            1, 1, _blockID, 1, list, stateRoot, MODERN_TIME + 1, 1, nullptr, _manager);


    return CommittedBlock::make(p->getSchainID(), p->getProposerNodeID(), p->getBlockID(),
                                p->getProposerIndex(), p->getTransactionList(), p->getStateRoot(), p->getTimeStampS(),
                                p->getTimeStampMs(), p->getSignature(), "EMPTY", "EMPTY");
}


ptr<BasicHeader> CommittedBlock::createBlockHeader() {
    return make_shared<CommittedBlockHeader>(*this);
}

string CommittedBlock::getThresholdSig() const {
    CHECK_STATE(!thresholdSig.empty());
    return thresholdSig;
}

string CommittedBlock::getDaSig() const {
    return daSig;
}

bool CommittedBlock::isLegacy() {
    return daSig.empty();
}

ptr<CommittedBlock> CommittedBlock::deserialize(
        const ptr<vector<uint8_t>> &_serializedBlock, const ptr<CryptoManager> &_manager,
        bool _verifySig) {
    CHECK_ARGUMENT(_serializedBlock);
    CHECK_ARGUMENT(_manager);

    serializedSanityCheck(_serializedBlock);

    string headerStr = extractHeader(_serializedBlock);

    CHECK_STATE(!headerStr.empty());

    ptr<CommittedBlockHeader> blockHeader;

    try {
        blockHeader = CommittedBlock::parseBlockHeader(headerStr);
        CHECK_STATE(blockHeader);

    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(ParsingException(
                "Could not parse committed block header: \n" + headerStr, __CLASS_NAME__));
    }


    ptr<TransactionList> list = nullptr;

    try {
        list = deserializeTransactions(blockHeader, headerStr, _serializedBlock);
    } catch (...) {
        throw_with_nested(
                InvalidStateException("Could not deserialize transactions", __CLASS_NAME__));
    }

    CHECK_STATE(list);

    ptr<CommittedBlock> block = nullptr;

    try {
        block = CommittedBlock::make(blockHeader->getSchainID(), blockHeader->getProposerNodeId(),
                                     blockHeader->getBlockID(), blockHeader->getProposerIndex(), list,
                                     blockHeader->getStateRoot(), blockHeader->getTimeStamp(),
                                     blockHeader->getTimeStampMs(),
                                     blockHeader->getSignature(), blockHeader->getThresholdSig(),
                                     blockHeader->getDaSig());
    } catch (...) {
        throw_with_nested(InvalidStateException("Could not make block", __CLASS_NAME__));
    }

    CHECK_STATE(block);

    if (!_verifySig) {
        return block;
    }

    // now verify block proposer signature and block signature
    // default blocks are not ecdsa signed
    if ((blockHeader->getProposerIndex() != 0)) {
        try {
            _manager->verifyProposalECDSA(block, blockHeader->getBlockHash(), blockHeader->getSignature());
        } catch (...) {
            LOG(err, "Block ECDSA signature did not verify in deserialization");
            throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
        }
    }

    try {
        block->verifyBlockSig(_manager);
    } catch (...) {
        LOG(err, "Block threshold signature did not verify in deserialization");
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

    try {
        if (!block->isLegacy())
            block->verifyDaSig(_manager);
    } catch (...) {
        LOG(err, "Block threshold signature did not verify in deserialization");
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

    return block;
}

ptr<CommittedBlockHeader> CommittedBlock::parseBlockHeader(
        const string &_header) {
    CHECK_ARGUMENT(!_header.empty());
    CHECK_ARGUMENT(_header.size() > 2);
    CHECK_ARGUMENT2(_header.at(0) == '{', "Block header does not start with {");
    CHECK_ARGUMENT2(
            _header.at(_header.size() - 1) == '}', "Block header does not end with }");

    auto js = nlohmann::json::parse(_header);

    return make_shared<CommittedBlockHeader>(js);
}

CommittedBlock::CommittedBlock(uint64_t timeStamp, uint32_t timeStampMs)
        : BlockProposal(timeStamp, timeStampMs) {}


CommittedBlock::CommittedBlock(const schain_id &_schainId, const node_id &_proposerNodeId,
                               const block_id &_blockId, const schain_index &_proposerIndex,
                               const ptr<TransactionList> &_transactions, const u256 &stateRoot, uint64_t timeStamp,
                               __uint32_t timeStampMs, const string &_signature, const string &_thresholdSig,
                               const string &_daSig)
        : BlockProposal(_schainId, _proposerNodeId, _blockId, _proposerIndex, _transactions, stateRoot,
                        timeStamp, timeStampMs, _signature, nullptr) {
    CHECK_ARGUMENT(_transactions);
    CHECK_ARGUMENT(!_signature.empty());
    CHECK_ARGUMENT(!_thresholdSig.empty());
    this->thresholdSig = _thresholdSig;
    this->daSig = _daSig;
}


ptr<vector<uint8_t> > CommittedBlock::serializeBlock() {

    LOCK(m)

    if (cachedSerializedBlock)
        return cachedSerializedBlock;

    auto blockHeader = createBlockHeader();

    CHECK_STATE(blockHeader);

    cachedSerializedBlock = serializeTransactionsAndCompleteSerialization(blockHeader);

    CHECK_STATE(cachedSerializedBlock);

    return cachedSerializedBlock;
}

void CommittedBlock::verifyBlockSig(ptr<CryptoManager> _cryptoManager) {

    CHECK_STATE(_cryptoManager)

    auto sig = getThresholdSig();

    auto hash = BLAKE3Hash::getBlockHash((uint64_t) getProposerIndex(),
                                         (uint64_t) getBlockID(),
                                         (uint64_t) getSchainID());
    try {
        _cryptoManager->verifyBlockSig(sig, getBlockID(), hash, getTimeStamp());
    } catch (InvalidSignatureException &) {
        throw_with_nested(
                InvalidStateException("Could not verify block BLS sig:", __CLASS_NAME__)
        );
    }

}

void CommittedBlock::verifyDaSig(ptr<CryptoManager> _cryptoManager) {

    CHECK_STATE(_cryptoManager)

    auto sig = getDaSig();

    auto hash = BLAKE3Hash::getBlockHash((uint64_t) getProposerIndex(),
                                         (uint64_t) getBlockID(),
                                         (uint64_t) getSchainID());
    try {
        _cryptoManager->verifyDAProofThresholdSig(hash, sig, getBlockID());
    } catch (InvalidSignatureException &) {
        throw_with_nested(
                InvalidStateException("Could not verify block BLS sig:", __CLASS_NAME__)
        );
    }

}


