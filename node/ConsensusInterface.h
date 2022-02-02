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
    @date 2018-
*/

#ifndef CONSENSUSINTERFACE_H
#define CONSENSUSINTERFACE_H

#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"

#include<boost/multiprecision/cpp_int.hpp>

#include "memory"
#include <string>
#include <vector>

enum consensus_engine_status {
    CONSENSUS_ACTIVE = 0, CONSENSUS_EXITED = 1,
};

#define TE_MAGIC_START "f84a1cf7214ae051cae8"
#define TE_MAGIC_END "98a773d884b2f1c4ac27"


using u256 = boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<256, 256,
        boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void> >;

/*
 * This is used by consensus to get the last argument of a call if transaction
 * is a call to a smartcontact. Will return true if:
 * 1. the transaction is a smart contract call AND
 * 2. the last argument of the call is array of bytes
 *
 * Otherwise will return false
 *
 * lastArgument - thats where the argument is copied if return value is true.
 */

class EncryptedTransactionAnalyzerInterface {
public:
    virtual std::shared_ptr<std::vector<uint8_t>> getEncryptedData(
            const std::vector<uint8_t>& transaction) = 0;
};

class EmptyEncryptedTransactionAnalyzerInterface : public EncryptedTransactionAnalyzerInterface {
public:

    std::shared_ptr<std::vector<uint8_t>> getEncryptedData(
            const std::vector<uint8_t>& ) override {
        return nullptr;
    }


};


class ConsensusInterface {
public:
    virtual ~ConsensusInterface() = default;

    virtual void parseFullConfigAndCreateNode(const std::string &fullPathToConfigFile,
                                              const std::string& gethURL,
                                              std::shared_ptr<EncryptedTransactionAnalyzerInterface> _analyzer)
                                              = 0;

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
#define ORACLE_INVALID_JSON_RESPONSE 8


    /*
     * Submit Oracle Request. This will return ORACLE_SUCCESS and a string receipt if everything
     * is. In case of an error, a non-zero error will be returned.
     */

    virtual uint64_t submitOracleRequest(const std::string& _spec, std::string &_receipt) = 0;

    /*
     * Check if Oracle result has been derived.  This will return ORACLE_SUCCESS if
     * nodes agreed on result. The signed result will be returned in _result string.
     *
     * If no result has been derived yet, ORACLE_RESULT_NOT_READY is returned.
     *
     * In case of an error, an error is returned.
     */


    virtual uint64_t  checkOracleResult(const std::string& _receipt, std::string& _result) = 0;

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
    virtual void createBlock(const transactions_vector &_approvedTransactions,
                             uint64_t _timeStamp,
                             uint32_t _timeStampMillis, uint64_t _blockID, u256 _gasPrice,
                             u256 _stateRoot, uint64_t _winningNodeIndex,
                             const std::shared_ptr<std::map<uint64_t, std::shared_ptr<std::vector<uint8_t>>>> decryptedArgs =
                                     nullptr) = 0;

    virtual ~ConsensusExtFace() = default;

    virtual void terminateApplication() {};


    /* Set sgx key info */

    void setSGXKeyInfo( std::string& _sgxServerURL,
                         // SSL key file full path. Can be null if the server does not require a client cert
                        std::string& _sgxSSLKeyFileFullPath,
                        // SSL cert file full path. Can be null if the server does not require a client cert
                        std::string& _sgxSSLCertFileFullPath,
                        // ecdsaKeyName of this node on the SGX server
                        std::string& _ecdsaKeyName,
                        // array of ECDSA public keys of all nodes, including this node
                        std::shared_ptr<std::vector<std::string>>& _ecdsaPublicKeys,
                        // blsKeyName of this node on the SGX server
                        std::string& _blsKeyName,
                       // array of BLS public key shares of all nodes, including this node
                       // each BLS public key share is a vector of 4 strings.
                        std::shared_ptr<std::vector<std::shared_ptr<std::vector<std::string>>>>& _blsPublicKeyShares);

};

#endif  // CONSENSUSINTERFACE_H
