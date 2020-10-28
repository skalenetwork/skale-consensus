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

    @file BlockProposal.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once

#include<boost/multiprecision/cpp_int.hpp>

#include "SkaleCommon.h"

#include "DataStructure.h"

class Schain;
class Transaction;
class PartialHashesList;
class TransactionList;
class  BLAKE3Hash;
class BlockProposalRequestHeader;
class CryptoManager;
class DAProof;
class BasicHeader;
class BlockProposalHeader;
class BlockProposalFragment;
class BlockProposalFragmentList;

class BlockProposal : public DataStructure {

    ptr<BlockProposalRequestHeader> header = nullptr;

    ptr<vector<uint8_t>> serializedProposal = nullptr;



protected:

    schain_id schainID;
    node_id proposerNodeID;
    block_id blockID;
    schain_index proposerIndex;
    transaction_count transactionCount;
    uint64_t  timeStamp = 0;
    uint32_t  timeStampMs = 0;
    u256 stateRoot = 0;

    ptr<TransactionList> transactionList = nullptr;
    ptr< BLAKE3Hash > hash = nullptr;
    string signature = "";

    void calculateHash();

    virtual ptr<BasicHeader> createHeader();

    static ptr<TransactionList> deserializeTransactions(const ptr<BlockProposalHeader>& _header,
                                                        const string& _headerString,
                                                        const ptr<vector<uint8_t>>& _serializedBlock);

    static string extractHeader(const ptr<vector<uint8_t>>& _serializedBlock);

    static ptr<BlockProposalHeader> parseBlockHeader(const string & _header );
public:


    BlockProposal(uint64_t _timeStamp, uint32_t _timeStampMs);

    BlockProposal(schain_id _sChainId, node_id _proposerNodeId, block_id _blockID,
                  schain_index _proposerIndex, const ptr<TransactionList>& _transactions, u256 _stateRoot,
                  uint64_t _timeStamp, __uint32_t _timeStampMs, const string& _signature,
                  const ptr<CryptoManager>& _cryptoManager);


    uint64_t getTimeStamp() const;

    uint32_t getTimeStampMs() const;


    schain_index getProposerIndex() const;

    node_id getProposerNodeID() const;

    ptr<BLAKE3Hash>getHash();

    ptr<PartialHashesList> createPartialHashesList();

    ptr<TransactionList> getTransactionList();

    block_id getBlockID() const;

    virtual ~BlockProposal();

    schain_id getSchainID() const;

    transaction_count getTransactionCount() const;

    void addSignature(const string& _signature);

    string  getSignature();

    static ptr<BlockProposalRequestHeader> createBlockProposalHeader(Schain* _sChain, const ptr<BlockProposal>& _proposal);


    ptr<vector<uint8_t> > serialize();


    static ptr<BlockProposal> deserialize(const ptr<vector<uint8_t> >& _serializedProposal,
                                                  const ptr<CryptoManager>& _manager);

    static ptr<BlockProposal> defragment(const ptr<BlockProposalFragmentList>& _fragmentList, const ptr<CryptoManager>& _cryptoManager);

    ptr<BlockProposalFragment> getFragment(uint64_t _totalFragments, fragment_index _index);

    u256 getStateRoot() const;

};

