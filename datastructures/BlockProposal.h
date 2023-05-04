/*
    Copyright (C) 2018- SKALE Labs

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

    @file BlockProposal.h
    @author Stan Kladko
    @date 2018 -
*/

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"

#include <boost/multiprecision/cpp_int.hpp>

#pragma GCC diagnostic pop

#include "SkaleCommon.h"

#include "DataStructure.h"
#include "TimeStamp.h"
#include "SendableItem.h"
#include "crypto/BLAKE3Hash.h"

class Schain;
class Transaction;
class PartialHashesList;
class TransactionList;
class BLAKE3Hash;
class BlockProposalRequestHeader;
class CryptoManager;
class DAProof;
class BasicHeader;
class BlockProposalHeader;
class BlockProposalFragment;
class BlockProposalFragmentList;

#define SERIALIZE_AS_PROPOSAL 1

class BlockProposal : public SendableItem {
    uint64_t creationTime;

    ptr< BlockProposalRequestHeader > cachedProposalRequestHeader = nullptr;  // tsafe

    ptr< vector< uint8_t > > cachedSerializedProposal = nullptr;  // tsafe

    ptr< BasicHeader > createProposalHeader();

    static atomic< int64_t > totalBlockProposalObjects;

protected:
    schain_id schainID = 0;
    node_id proposerNodeID = 0;
    block_id blockID = 0;
    schain_index proposerIndex = 0;
    transaction_count transactionCount = 0;
    uint64_t timeStamp = 0;
    uint32_t timeStampMs = 0;
    u256 stateRoot = 0;

    ptr< TransactionList > transactionList = nullptr;  // tsafe

    BLAKE3Hash hash;  // tsafe

    string signature;

    void calculateHash();


    ptr< vector< uint8_t > > serializeTransactionsAndCompleteSerialization(
        ptr< BasicHeader > _blockHeader );

    static ptr< TransactionList > deserializeTransactions(
        const ptr< BlockProposalHeader >& _header, const string& _headerString,
        const ptr< vector< uint8_t > >& _serializedBlock );

    static string extractHeader( const ptr< vector< uint8_t > >& _serializedBlock );

    static ptr< BlockProposalHeader > parseBlockHeader( const string& _header );

public:
    BlockProposal( uint64_t _timeStamp, uint32_t _timeStampMs );

    BlockProposal( schain_id _sChainId, node_id _proposerNodeId, block_id _blockID,
        schain_index _proposerIndex, const ptr< TransactionList >& _transactions, u256 _stateRoot,
        uint64_t _timeStamp, __uint32_t _timeStampMs, const string& _signature,
        const ptr< CryptoManager >& _cryptoManager );

    [[nodiscard]] uint64_t getTimeStampS() const;

    [[nodiscard]] uint32_t getTimeStampMs() const;

    [[nodiscard]] TimeStamp getTimeStamp() const;

    [[nodiscard]] schain_index getProposerIndex() const;

    [[nodiscard]] node_id getProposerNodeID() const;

    BLAKE3Hash getHash();

    ptr< PartialHashesList > createPartialHashesList();

    ptr< TransactionList > getTransactionList();

    [[nodiscard]] block_id getBlockID() const;

    ~BlockProposal() override;

    [[nodiscard]] schain_id getSchainID() const;

    [[nodiscard]] transaction_count getTransactionCount() const;

    void addSignature( const string& _signature );

    string getSignature();

    ptr< vector< uint8_t > > serializeProposal();

    ptr< BlockProposalFragment > getFragment( uint64_t _totalFragments, fragment_index _index );

    [[nodiscard]] u256 getStateRoot() const;

    ptr< BlockProposalRequestHeader > createProposalRequestHeader( Schain* _sChain );

    static ptr< BlockProposal > deserialize( const ptr< vector< uint8_t > >& _serializedProposal,
        const ptr< CryptoManager >& _manager, bool _verifySig );

    static ptr< BlockProposal > defragment( const ptr< BlockProposalFragmentList >& _fragmentList,
        const ptr< CryptoManager >& _cryptoManager );

    uint64_t getCreationTime() const;

    static uint64_t getTotalObjects();
};
