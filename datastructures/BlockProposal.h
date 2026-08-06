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
class BiteCiphertext;

#define SERIALIZE_AS_PROPOSAL 1

#ifdef BITE
#include "bite/crypto/TransactionCiphertexts.h"
#include "abstracttcpserver/ConnectionStatus.h"
class AESKeyDecryptionShareList;
class TransactionCiphertextsMap;
#endif


class BlockProposal : public SendableItem {
    uint64_t creationTime;

    ptr<BlockProposalRequestHeader> cachedProposalRequestHeader = nullptr; // tsafe


    ptr<BasicHeader> createProposalHeader();

    static atomic<int64_t> totalBlockProposalObjects;

protected:
    ptr<vector<uint8_t> > cachedSerializedProposal = nullptr; // tsafe

    schain_id schainID = 0;
    node_id proposerNodeID = 0;
    block_id blockID = 0;
#ifdef BITE
    epoch_id epochID = 0;
#endif
    schain_index proposerIndex = 0;
    transaction_count transactionCount = 0;
    uint64_t timeStamp = 0;
    uint32_t timeStampMs = 0;
    u256 stateRoot = 0;

    ptr<TransactionList> transactionList = nullptr; // tsafe

    BLAKE3Hash hash; // tsafe

    string signature;

    void calculateHash();


    ptr<vector<uint8_t> > serializeTransactionsAndCompleteSerialization(
        ptr<BasicHeader> _blockHeader);

    static ptr<TransactionList> deserializeTransactions(
        const ptr<BlockProposalHeader> &_header, const string &_headerString,
        const ptr<vector<uint8_t> > &_serializedBlock);

    static string extractHeader(const ptr<vector<uint8_t> > &_serializedBlock);


    BlockProposal(uint64_t _timeStamp, uint32_t _timeStampMs);

    BlockProposal(schain_id _sChainId, node_id _proposerNodeId, block_id _blockID,
#ifdef BITE
        epoch_id _epochID,
#endif

                  schain_index _proposerIndex, const ptr<TransactionList> &_transactions, u256 _stateRoot,
                  uint64_t _timeStamp, __uint32_t _timeStampMs, const string &_signature,
                  const ptr<CryptoManager> &_cryptoManager);


    static ptr<BlockProposal> deserialize(const ptr<vector<uint8_t> > &_serializedProposal,
                                          const ptr<CryptoManager> &_manager, bool _verifySig);

public:
    static ptr<BlockProposalHeader> parseBlockHeader(const string_view &_header);

    static ptr<BlockProposal> makeFromNetworkSerialized(const ptr<vector<uint8_t> > &_serializedProposal,
                                                        const ptr<CryptoManager> &_manager);

    static ptr<BlockProposal> makeFromDBSerialized(const ptr<vector<uint8_t> > &_serializedProposal,
                                                    const ptr<CryptoManager> &_manager);



    void setCachedSerializedProposal(const ptr<vector<uint8_t> > &_cachedSerializedProposal);

    static ptr<BlockProposal> makeFromSerialized(schain_id _sChainId, node_id _proposerNodeId,
                                                 block_id _blockID,
#ifdef BITE
                                                 epoch_id _epochID,
#endif

                                                 schain_index _proposerIndex,
                                                 const ptr<TransactionList> &_transactions,
                                                 u256 _stateRoot, uint64_t _timeStamp, __uint32_t _timeStampMs,
                                                 const string &_signature,
                                                 const ptr<CryptoManager> &_cryptoManager);


    [[nodiscard]] uint64_t getTimeStampS() const;

    [[nodiscard]] uint32_t getTimeStampMs() const;

    [[nodiscard]] TimeStamp getTimeStamp() const;

    [[nodiscard]] schain_index getProposerIndex() const;

    [[nodiscard]] node_id getProposerNodeID() const;

    BLAKE3Hash getHash();

    ptr<PartialHashesList> createPartialHashesList();

    ptr<TransactionList> getTransactionList();

    [[nodiscard]] block_id getBlockID() const;

#ifdef BITE
    [[nodiscard]] epoch_id getEpochID() const;
#endif

    ~BlockProposal() override;

    [[nodiscard]] schain_id getSchainID() const;

    [[nodiscard]] transaction_count getTransactionCount() const;

    void addSignature(const string &_signature);

    string getSignature();

    ptr<vector<uint8_t> > serializeProposal();

    ptr<BlockProposalFragment> getFragment(uint64_t _totalFragments, fragment_index _index
#ifdef BITE
                                           , schain_index _decryptorIndex
                                           , ptr< AESKeyDecryptionShareList > _decryptionShares
#endif
    );

    [[nodiscard]] u256 getStateRoot() const;

    ptr<BlockProposalRequestHeader> createProposalRequestHeader(Schain *_sChain);


    static ptr<BlockProposal> defragment(const ptr<BlockProposalFragmentList> &_fragmentList,
                                         const ptr<CryptoManager> &_cryptoManager);

    uint64_t getCreationTime() const;

    static uint64_t getTotalObjects();

#ifdef BITE

    enum class MyDecryptionSharesState {
        NotStarted,  // not yet scheduled
        InProgress,  // scheduled and waiting for result
        Ready,       // ready and available
        Failed       // decryption shares could not be computed
    };

    // For BITE protocol when a node receives a block proposal, it verifies and decrypts BITE shares for this proposal
    // using the SGX server
    // the resulting AESKeyDecryptionShareList is then saved together with the block proposal.

private:
    ptr<AESKeyDecryptionShareList> myDecryptionShares = nullptr;

    atomic<MyDecryptionSharesState> myDecryptionSharesState = MyDecryptionSharesState::NotStarted;
    // this condition variable is notified when myDecryptionSharesState changes to Ready or Failed
    condition_variable_any myDecryptionSharesCond;

    // Stores 1 or more ciphertexts associated with some transaction index
    ptr<TransactionCiphertextsMap> transactionCiphertexts = nullptr;

    // this will normally be empty
    map<transaction_index, ConnectionSubStatus> failedTransactions;
    // the encrypted AES key batch to send to SGX server
    ptr<vector<string>> sgxAESKeyBatch;

public:
    [[nodiscard]] map<transaction_index, ConnectionSubStatus>& getFailedTransactionsRef() {
        return failedTransactions;
    }


    [[nodiscard]] ptr<AESKeyDecryptionShareList> getMyDecryptionShares() const {
        if (myDecryptionSharesState.load(std::memory_order_acquire) !=
            MyDecryptionSharesState::Ready) {
            return nullptr;
        }
        auto result = std::atomic_load(&myDecryptionShares);
        return result;
    }

    [[nodiscard]] MyDecryptionSharesState getMyDecryptionSharesState() const {
        return myDecryptionSharesState.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool tryBeginMyDecryptionSharesComputation();

    void markMyDecryptionSharesReady(const ptr<AESKeyDecryptionShareList> &_myDecryptionShares);

    void markMyDecryptionSharesFailed();

    bool waitUntilMyDecryptionSharesResolved(uint64_t _timeoutMs = BITE_LOCAL_DECRYPTION_SHARES_WAIT_TIMEOUT_MS);


    [[nodiscard]] ptr<vector<string>> getSGXAESKeyBatch() const {
        auto result = std::atomic_load(&sgxAESKeyBatch);
        return result;
    }

    void setSGXAESKeyBatch(ptr<vector<string>>  _sgxAESKeyBatch) {
        CHECK_STATE(!sgxAESKeyBatch)
        sgxAESKeyBatch = _sgxAESKeyBatch;
    }


    void setMyDecryptionShares(const ptr<AESKeyDecryptionShareList> &_myDecryptionShares);


    void setTransactionCiphertexts(ptr<TransactionCiphertextsMap> _EncryptedAESKeyMap) {
        CHECK_STATE(_EncryptedAESKeyMap)
        // verify we are not setting it twice
        CHECK_STATE(std::atomic_exchange(&transactionCiphertexts, _EncryptedAESKeyMap) == nullptr);
    }


    [[nodiscard]] ptr<TransactionCiphertextsMap> getTransactionCiphertexts() const {
        auto result = std::atomic_load(&transactionCiphertexts);
        CHECK_STATE(transactionCiphertexts);
        return result;
    }
#endif
};
