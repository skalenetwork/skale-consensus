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

    @file CommittedBlock.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#define BOOST_PENDING_INTEGER_LOG2_HPP
#include <boost/integer/integer_log2.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "BlockProposalFragmentList.h"

#include "BlockProposal.h"

class Schain;
class BlockProposalHeader;
class ThresholdSignature;
class CommittedBlockHeader;

class BlockProposalFragment;

class CommittedBlock : public BlockProposal {

    string thresholdSig;
    string daSig;

    ptr<vector<uint8_t> > cachedSerializedBlock = nullptr;  // tsafe

    bool isLegacy();


    static ptr<CommittedBlockHeader> parseBlockHeader(const string &_header);

    ptr<BasicHeader> createBlockHeader();

public:
    CommittedBlock(uint64_t timeStamp, uint32_t timeStampMs);


    CommittedBlock(const schain_id &_schainId, const node_id &_proposerNodeId,
                   const block_id &_blockId, const schain_index &_proposerIndex,
                   const ptr<TransactionList> &_transactions, const u256 &_stateRoot, uint64_t _timeStamp,
                   __uint32_t _timeStampMs, const string &_signature, const string &_thresholdSig, const string &_daSig);

    [[nodiscard]] string getThresholdSig() const;

    [[nodiscard]] string getDaSig() const;


    static ptr<CommittedBlock>
    makeFromProposal(const ptr<BlockProposal> &_proposal, const ptr<ThresholdSignature> &_thresholdSig,
                     ptr<ThresholdSignature> _daSig);

    static ptr<CommittedBlock> make(schain_id _sChainId, node_id _proposerNodeId,
                                    block_id _blockId, schain_index _proposerIndex,
                                    const ptr<TransactionList> &_transactions,
                                    const u256 &_stateRoot, uint64_t _timeStamp, uint64_t _timeStampMs,
                                    const string &_signature, const string &_thresholdSig, const string &_daSig);


    static ptr<CommittedBlock> deserialize(
            const ptr<vector<uint8_t>> &_serializedBlock, const ptr<CryptoManager> &_manager,
            bool _verifySig);


    static ptr<CommittedBlock> createRandomSample(const ptr<CryptoManager> &_manager,
                                                  uint64_t _size, boost::random::mt19937 &_gen,
                                                  boost::random::uniform_int_distribution<> &_ubyte,
                                                  block_id _blockID = block_id(1));

    static void serializedSanityCheck(const ptr<vector<uint8_t>> &_serializedBlock);

    ptr<vector<uint8_t> > serialize();


    void verifyBlockSig(ptr<CryptoManager> _cryptoManager);

    void verifyDaSig(ptr<CryptoManager> _cryptoManager);



};