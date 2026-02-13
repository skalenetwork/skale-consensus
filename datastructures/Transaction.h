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

    @file Transaction.h
    @author Stan Kladko
    @date 2018
*/

#pragma once


#define BOOST_PENDING_INTEGER_LOG2_HPP
#include <boost/integer/integer_log2.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include "crypto/BLAKE3Hash.h"
#include "rlp/ParsedEthTransaction.h"

#include "datastructures/DataStructure.h"

#ifdef BITE
#include "node/ConsensusInterface.h"
#endif

class BLAKE3Hash;


#ifdef BITE
class BiteCiphertext;
class ParsedEthTransaction;
#endif

class Transaction : public DataStructure {
    bool haveHash = false;

    static atomic< int64_t > totalObjects;

    ptr< vector< uint8_t > > data = nullptr;

    BLAKE3Hash hash;

    ptr< partial_sha_hash > partialHash = nullptr;

#ifdef BITE
    // --------- Cached Fields -----------

    // Stores transaction bytes parsed as Ethereum transaction
    ptr<ParsedEthTransaction> parsedAndValidatedEthTransaction = nullptr;
    // Stores parsed BITE ciphertext for regular transactions
    ptr<BiteCiphertext> parsedEncryptedRegularTx = nullptr;
    
#ifdef BITE2
    // Stores a list of encrypted arguments from CAT transaction
    ptr<std::vector<ptr<BiteCiphertext>>> parsedEncryptedCATArgs = nullptr;

    // stores the 'to' field of the CTX transaction as AAD for TE
    // using ptr to allow atomic store/load
    ptr<AddressBytes> scAddressAadTE;
#endif

#endif
public:
    Transaction( const ptr< vector< uint8_t > >& _data, bool _includesPartialHash );

    void validate();

    uint64_t getSerializedSize( bool _writePartialHash );


    ptr< vector< uint8_t > > getData() const;


    void serializeInto( const ptr< vector< uint8_t > >& _out, bool _writePartialHash );


    BLAKE3Hash getHash();

    ptr< partial_sha_hash > getPartialHash();

    virtual ~Transaction();


    static ptr< Transaction > deserialize( const ptr< vector< uint8_t > >& _data,
        uint64_t _startIndex, uint64_t _len, bool _verifyPartialHashes );


    static int64_t getTotalObjects() { return totalObjects; };

    static ptr< Transaction > createRandomSample( uint64_t _size, boost::random::mt19937& _gen,
        boost::random::uniform_int_distribution<>& _ubyte );

#ifdef BITE
    // parses the data bytes of current transaction as an Ethereum RLP-encoded transaction 
    ptr<ParsedEthTransaction> getAsEthereumTransaction();

    // Allows caching parsed encrypted regular transactions' data field
    ptr<BiteCiphertext> getRegularTxEncryptedData();
    void setRegularTxEncryptedData( ptr<BiteCiphertext> _biteDataField );

#ifdef BITE2
    // Allows caching parsed encrypted CAT transaction arguments
    ptr<std::vector<ptr<BiteCiphertext>>> getCTXEncryptedArgs();
    void setCTXEncryptedArgs( ptr<std::vector<ptr<BiteCiphertext>>> _biteDataField );

    void setScAddressAadTE( const AddressBytes& _scAddressAadTE );
    ptr<AddressBytes> getScAddressAadTE();
#endif

    ptr<vector<uint8_t>> emplaceAndReencodeTransaction(vector<uint8_t>& _originalDataField );
#endif

};
