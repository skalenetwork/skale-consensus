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

#include<boost/multiprecision/cpp_int.hpp>

#include <map>
#include <string>
#include <vector>

enum consensus_engine_status {
    CONSENSUS_ACTIVE = 0, CONSENSUS_EXITED = 1,
};



using u256 = boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<256, 256,
        boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void> >;

class ConsensusInterface {
public:
    virtual ~ConsensusInterface() = default;

    virtual void parseFullConfigAndCreateNode(const std::string &fullPathToConfigFile,
                                              const string& gethURL) = 0;


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

     Consensus guarantees that it will not do anything for a particular block ID, until pendingTransactions(...)
     for this block id returns.
     */

    virtual void exitGracefully() = 0;

    virtual u256 getPriceForBlockId(uint64_t _blockId) const = 0;

    virtual u256 getRandomForBlockId(uint64_t _blockId) const = 0;

    virtual map< string, uint64_t > getConsensusDbUsage() const = 0;

    virtual uint64_t getEmptyBlockIntervalMs() const { return -1; }

    virtual void setEmptyBlockIntervalMs(uint64_t) {}

    virtual consensus_engine_status getStatus() const = 0;

#define ORACLE_SUCCESS  0
#define ORACLE_UNKNOWN_RECEIPT  1
#define ORACLE_TIMEOUT 2
#define ORACLE_NO_CONSENSUS  3
#define ORACLE_UNKNOWN_ERROR  4
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
#define ORACLE_JSPS_NOT_PROVIDED  36
#define ORACLE_JSPS_NOT_ARRAY  37
#define ORACLE_JSPS_EMPTY  38
#define ORACLE_TOO_MANY_JSPS  39
#define ORACLE_JSP_TOO_LONG  40
#define ORACLE_JSP_NOT_STRING  41
#define ORACLE_TRIMS_ITEM_NOT_STRING  42
#define ORACLE_JSPS_TRIMS_SIZE_NOT_EQUAL 43
#define ORACLE_POST_NOT_STRING 44
#define ORACLE_POST_STRING_TOO_LARGE 45
#define ORACLE_NO_PARAMS_ETH_CALL 46
#define ORACLE_PARAMS_ARRAY_INCORRECT_SIZE 47
#define ORACLE_PARAMS_ARRAY_FIRST_ELEMENT_NOT_OBJECT 48
#define ORACLE_PARAMS_INVALID_FROM_ADDRESS 49
#define ORACLE_PARAMS_INVALID_TO_ADDRESS 50
#define  ORACLE_PARAMS_ARRAY_INCORRECT_COUNT 51
#define ORACLE_BLOCK_NUMBER_NOT_STRING 52
#define ORACLE_INVALID_BLOCK_NUMBER 53
#define ORACLE_MISSING_FIELD 54
#define ORACLE_INVALID_FIELD 55
#define ORACLE_EMPTY_JSON_RESPONSE 56
#define ORACLE_COULD_NOT_PROCESS_JSPS_IN_JSON_RESPONSE 57


    /*
     * Submit Oracle Request. This will return ORACLE_SUCCESS and a string receipt if everything
     * is. In case of an error, a non-zero error will be returned.
     * Note: this functions is guaranteed to not throw exceptions
     *  Error values are enumerated above
     */

    virtual uint64_t submitOracleRequest(const string& _spec, string &_receipt, string& _errorMessage) = 0;

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


    virtual uint64_t  checkOracleResult(const string& _receipt, string& _result) = 0;


    /*
     * This will return a consensus block serialized as byte array from consensus db.
     * Returns nullptr if the block is not in consensus DB
     */
    //virtual std::shared_ptr<std::vector<std::uint8_t>> getSerializedBlock(
      //  std::uint64_t _blockNumber)  = 0;

};

/**
 * Through this interface Consensus interacts with the rest of the system
 */
class ConsensusExtFace {
public:
    typedef std::vector<std::vector<uint8_t> > transactions_vector;

    // Returns hashes and bytes of new transactions as well as state root to put into block proposal
    virtual transactions_vector pendingTransactions(size_t _limit, u256& _stateRoot) = 0;

    // Creates new block with specified transactions AND removes them from the queue
    virtual void createBlock(const transactions_vector &_approvedTransactions, uint64_t _timeStamp,
                             uint32_t _timeStampMillis, uint64_t _blockID, u256 _gasPrice,
                             u256 _stateRoot, uint64_t _winningNodeIndex) = 0;

    virtual ~ConsensusExtFace() = default;

    virtual void terminateApplication() {};

};

#endif  // CONSENSUSINTERFACE_H
