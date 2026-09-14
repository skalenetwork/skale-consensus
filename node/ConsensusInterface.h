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

    @file ConsensusInterface.h
    @author Stan Kladko
    @date 2018
*/

#ifndef CONSENSUSINTERFACE_H
#define CONSENSUSINTERFACE_H

#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"

#include <boost/multiprecision/cpp_int.hpp>

#pragma GCC diagnostic pop

#include "node/ConsensusTypes.h"

enum consensus_engine_status {
    CONSENSUS_ACTIVE = 0,
    CONSENSUS_EXITED = 1,
};

using u256 = boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<256,
        256, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void> >;

class ConsensusInterface {
public:
    virtual ~ConsensusInterface() = default;

    virtual void parseFullConfigAndCreateNode(
        const std::string &fullPathToConfigFile
        , const std::string &gethURL
    ) = 0;


    // If starting from a snapshot, start all will pass to consensus the last comitted
    // block coming from the snapshot. Normally, nullptr is passed.
    virtual void startAll() = 0;

    virtual void bootStrapAll() = 0;


    /* exitGracefully is asyncronous and returns immediately
     you are supposed to
     a) Receive an exit request from the user
     b) finish current block processing
     c) call exitGracefully() to tell consensus it needs to exit
     d) unblock (return from) pendingTransactions(...) function returning an empty transaction
       vector
     e) If you are in createBlock() function, return from it too (it is void)
     f) call getStatus() from time to time until it returns CONSENSUS_EXITED

     Consensus guarantees that it will not do anything for a particular block ID, until
     pendingTransactions(...) for this block id returns.
     */

    virtual void exitGracefully() = 0;

    virtual u256 getPriceForBlockId(uint64_t _blockId) const = 0;

    virtual u256 getRandomForBlockId(uint64_t _blockId) const = 0;

#ifdef BITE
    /**
     * Returns a random for a block id that is derived from the threshold signature of the block 
     * with a custom domain. The merged signature is never stored on chain and is only used 
     * locally by nodes, so the resulting random is not publicly reproducible.
     * The generated random is deterministic - all nodes will generate the same random for a 
     * given block id, but it cannot be reproduced by external observers.
     */
    virtual u256 getReencryptionRandomForBlockId(uint64_t _blockId) const = 0;
#endif

    virtual std::map<std::string, uint64_t> getConsensusDbUsage() const = 0;

    virtual uint64_t getEmptyBlockIntervalMs() const { return -1; }

    virtual void setEmptyBlockIntervalMs(uint64_t) {}

    virtual consensus_engine_status getStatus() const = 0;

    virtual void setPaused(bool _paused) = 0;


#define ORACLE_SUCCESS 0
#define ORACLE_UNKNOWN_RECEIPT 1
#define ORACLE_TIMEOUT 2
#define ORACLE_NO_CONSENSUS 3
#define ORACLE_UNKNOWN_ERROR 4
#define ORACLE_RESULT_NOT_READY 5
#define ORACLE_DUPLICATE_REQUEST 6
#define ORACLE_COULD_NOT_CONNECT_TO_ENDPOINT 7
#define ORACLE_ENDPOINT_JSON_RESPONSE_COULD_NOT_BE_PARSED 8
#define ORACLE_INTERNAL_SERVER_ERROR 9
#define ORACLE_INVALID_JSON_REQUEST 10
#define ORACLE_TIME_IN_REQUEST_SPEC_TOO_OLD 11
#define ORACLE_TIME_IN_REQUEST_SPEC_IN_THE_FUTURE 11
#define ORACLE_INVALID_CHAIN_ID 12
#define ORACLE_REQUEST_TOO_LARGE 13
#define ORACLE_RESULT_TOO_LARGE 14
#define ORACLE_ETH_METHOD_NOT_SUPPORTED 15
#define ORACLE_URI_TOO_SHORT 16
#define ORACLE_URI_TOO_LONG 17
#define ORACLE_UNKNOWN_ENCODING 18
#define ORACLE_INVALID_URI_START 19
#define ORACLE_INVALID_URI 20
#define ORACLE_USERNAME_IN_URI 21
#define ORACLE_PASSWORD_IN_URI 22
#define ORACLE_IP_ADDRESS_IN_URI 23
#define ORACLE_UNPARSABLE_SPEC 24
#define ORACLE_NO_CHAIN_ID_IN_SPEC 25
#define ORACLE_NON_UINT64_CHAIN_ID_IN_SPEC 26
#define ORACLE_NO_URI_IN_SPEC 27
#define ORACLE_NON_STRING_URI_IN_SPEC 28
#define ORACLE_NO_ENCODING_IN_SPEC 29
#define ORACLE_NON_STRING_ENCODING_IN_SPEC 30
#define ORACLE_TIME_IN_SPEC_NO_UINT64 31
#define ORACLE_POW_IN_SPEC_NO_UINT64 32
#define ORACLE_POW_DID_NOT_VERIFY 33
#define ORACLE_ETH_API_NOT_STRING 34
#define ORACLE_ETH_API_NOT_PROVIDED 35
#define ORACLE_JSPS_NOT_PROVIDED 36
#define ORACLE_JSPS_NOT_ARRAY 37
#define ORACLE_JSPS_EMPTY 38
#define ORACLE_TOO_MANY_JSPS 39
#define ORACLE_JSP_TOO_LONG 40
#define ORACLE_JSP_NOT_STRING 41
#define ORACLE_TRIMS_ITEM_NOT_STRING 42
#define ORACLE_JSPS_TRIMS_SIZE_NOT_EQUAL 43
#define ORACLE_POST_NOT_STRING 44
#define ORACLE_POST_STRING_TOO_LARGE 45
#define ORACLE_NO_PARAMS_ETH_CALL 46
#define ORACLE_PARAMS_ARRAY_INCORRECT_SIZE 47
#define ORACLE_PARAMS_ARRAY_FIRST_ELEMENT_NOT_OBJECT 48
#define ORACLE_PARAMS_INVALID_FROM_ADDRESS 49
#define ORACLE_PARAMS_INVALID_TO_ADDRESS 50
#define ORACLE_PARAMS_ARRAY_INCORRECT_COUNT 51
#define ORACLE_BLOCK_NUMBER_NOT_STRING 52
#define ORACLE_INVALID_BLOCK_NUMBER 53
#define ORACLE_MISSING_FIELD 54
#define ORACLE_INVALID_FIELD 55
#define ORACLE_EMPTY_JSON_RESPONSE 56
#define ORACLE_COULD_NOT_PROCESS_JSPS_IN_JSON_RESPONSE 57
#define ORACLE_NO_TIME_IN_SPEC 58
#define ORACLE_NO_POW_IN_SPEC 59
#define ORACLE_HSPS_TRIMS_SIZE_NOT_EQUAL 60
#define ORACLE_PARAMS_NO_ARRAY 61
#define ORACLE_PARAMS_GAS_NOT_UINT64 62

    /*
     * Submit Oracle Request. This will return ORACLE_SUCCESS and a string receipt if everything
     * is. In case of an error, a non-zero error will be returned.
     * Note: this functions is guaranteed to not throw exceptions
     *  Error values are enumerated above
     */

    virtual uint64_t submitOracleRequest(
            const std::string &_spec, std::string &_receipt, std::string &_errorMessage) = 0;

    /*
     * Check if Oracle result has been derived.  This will return ORACLE_SUCCESS if
     * nodes agreed on result. The signed result will be returned in _result string.
     *
     * If no result has been derived yet, ORACLE_RESULT_NOT_READY is returned.
     *
     * In case of an error, an error is returned.
     * Note: this functions is guaranteed to not throw exceptions
     * Error values are enumerated above
     */


    virtual uint64_t checkOracleResult(const std::string &_receipt, std::string &_result) = 0;

    struct SyncInfo {
        // sync information as required by eth_syncing API request of geth
        bool isSyncing = false;
        std::uint64_t startingBlock = 0;
        std::uint64_t currentBlock = 0;
        std::uint64_t highestBlock = 0;

        std::string toString() {
            return std::to_string(isSyncing) + ":" + std::to_string(startingBlock) + ":" +
            std::to_string(currentBlock) + ":" + std::to_string(highestBlock);
        }
    };

    // return sync information as requested by eth_syncing API of geth
    // if isSyncing is false, all fields will be set to zero.
    virtual SyncInfo getSyncInfo() = 0;

#ifdef FAIR
    virtual void updateLogger() const = 0;
#endif

};


/**
 * Through this interface Consensus interacts with the rest of the system
 */
class ConsensusExtFace {
public:

    // BITE2 pending transactions include both regular and CAT transactions.
    // CAT should be placed before regular transactions.
    struct Transactions {
    private:
        transactions_vector all;
        
#ifdef BITE
        // number of CAT txs in 'all' vector
        std::size_t ctxsSize = 0;
#endif

        using iterator = typename transactions_vector::iterator;
        using const_iterator = typename transactions_vector::const_iterator;

    public:

#ifdef BITE
        std::size_t sizeCTX() const noexcept {
            return ctxsSize;
        }

        bool isCTX(size_t index) const {
            return index < ctxsSize;
        }

        void emplaceBackCTX(Bytes&& b) {
            CHECK_STATE(ctxsSize == all.size()); // ensure all cats are contiguous at the start
            all.emplace_back(std::move(b));
            ctxsSize++;
        }

        void pushBackCTX(const Bytes &b) {
            CHECK_STATE(ctxsSize == all.size());
            all.push_back(b);
            ctxsSize++;
        }
#endif

        Bytes at(size_t index) const {
            return all.at(index);
        }

        bool empty() const noexcept {
            return all.empty();
        }

        std::size_t size() const noexcept {
            return all.size();
        }

        iterator begin() {
            return all.begin();
        }

        iterator end() {
            return all.end();
        }

        const_iterator begin() const {
            return all.begin();
        }

        const_iterator end() const {
            return all.end();
        }

        void emplaceBackRegular(Bytes&& b) {
            all.emplace_back(std::move(b));
        }

        void pushBackRegular(const Bytes &b) {
            all.push_back(b);
        }
    };


    // Returns hashes and bytes of new transactions as well as state root to put into block proposal
    virtual Transactions pendingTransactions(size_t _limit, u256 &_stateRoot) = 0;

    // Creates new block with specified transactions AND removes them from the queue
    virtual void createBlock(const Transactions &_approvedTransactions,
#ifdef BITE
        DecryptedTransactions _decryptedTransactions,
#endif
        uint64_t _timeStamp, uint32_t _timeStampMillis, uint64_t _blockID, u256 _gasPrice, u256 _stateRoot,
                             uint64_t _winningNodeIndex) = 0;



    virtual ~ConsensusExtFace() = default;

    virtual void terminateApplication() {};
};

#endif  // CONSENSUSINTERFACE_H
