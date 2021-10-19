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

    virtual void parseFullConfigAndCreateNode(const std::string &fullPathToConfigFile) = 0;

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


    /* Set sgx key info */

    void setSGXKeyInfo( string& _sgxServerURL,
                         // SSL key file full path. Can be null if the server does not require a client cert
                        string& _sgxSSLKeyFileFullPath,
                        // SSL cert file full path. Can be null if the server does not require a client cert
                        string& _sgxSSLCertFileFullPath,
                        // ecdsaKeyName of this node on the SGX server
                        string& _ecdsaKeyName,
                        // array of ECDSA public keys of all nodes, including this node
                        shared_ptr<vector<string>>& _ecdsaPublicKeys,
                        // blsKeyName of this node on the SGX server
                        string& _blsKeyName,
                       // array of BLS public key shares of all nodes, including this node
                       // each BLS public key share is a vector of 4 strings.
                        shared_ptr<vector<shared_ptr<vector<string>>>>& _blsPublicKeyShares);

};

#endif  // CONSENSUSINTERFACE_H
