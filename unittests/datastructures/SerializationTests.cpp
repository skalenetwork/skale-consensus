/*
    Copyright (C) 2019 SKALE Labs

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

    @file SerializationTests.cpp
    @author Stan Kladko
    @date 2019
*/


#include "openssl/bio.h"

#include "openssl/evp.h"
#include "openssl/pem.h"
#include "openssl/err.h"
#include "openssl/ec.h"


#include "SkaleCommon.h"
#include "exceptions/ParsingException.h"
#include "crypto/CryptoManager.h"
#ifdef BITE
#include "bite/BiteManager.h"
#endif
#include "chains/Schain.h"

#include "datastructures/CommittedBlock.h"
#include "datastructures/CommittedBlockList.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"
#include "datastructures/BlockProposalFragment.h"
#include "datastructures/BlockProposalFragmentList.h"


#define BOOST_PENDING_INTEGER_LOG2_HPP

#include <boost/integer/integer_log2.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "thirdparty/catch.hpp"


void corrupt_byte_vector( const ptr< vector< uint8_t > >& _in, boost::random::mt19937& _gen,
    boost::random::uniform_int_distribution<>& _ubyte ) {
    int randomPosition = _ubyte( _gen ) % _in->size();
    auto b = _in->at( randomPosition );
    _in->at( randomPosition ) = b + 1;
}


void test_committed_block_fragment_defragment( bool _fail ) {
    boost::random::mt19937 gen;

    boost::random::uniform_int_distribution<> ubyte( 0, 255 );

    ConsensusEngine engine( 0, 100000000 );

    Schain chain;

    auto cryptoManager = make_shared< CryptoManager >( chain );


    for ( int i = 1; i < 200; i++ ) {
        auto t = CommittedBlock::createRandomSample( cryptoManager, i, gen, ubyte, i );

        auto list = make_shared< BlockProposalFragmentList >( i, i );


        uint64_t next;

        for ( int j = 1; j < i; j++ ) {
            next = 0;
            list->addFragment( t->getFragment( i, j
#ifdef BITE
                                   , 1, nullptr
#endif
                                   ));
            next = list->nextIndexToRetrieve();
            CATCH_REQUIRE( next != 0 );
        }


        list->addFragment( t->getFragment( i, i
#ifdef BITE
                               , 1, nullptr
#endif
                               ) );
        next = list->nextIndexToRetrieve();
        CATCH_REQUIRE( next == 0 );

        CATCH_REQUIRE( list->isComplete() );


        // auto out = t->getSerialized();

        if ( _fail ) {
            // corrupt_byte_vector( out, gen, ubyte );
        }


        // CATCH_REQUIRE( out != nullptr );

        if ( _fail ) {
            CATCH_REQUIRE_THROWS( BlockProposal::defragment( list, cryptoManager ) );
        } else {
            ptr< BlockProposal > imp = nullptr;

            try {
                imp = BlockProposal::defragment( list, cryptoManager );
            } catch ( SkaleException& e ) {
                SkaleException::logNested( e, err );
                throw( e );
            }
            CATCH_REQUIRE( imp );

            CATCH_REQUIRE( *imp->serializeProposal() == *t->serializeProposal() );
        }
    }
}


void test_tx_serialize_deserialize( bool _fail ) {
    boost::random::mt19937 gen;

    boost::random::uniform_int_distribution<> ubyte( 0, 255 );

    for ( int k = 0; k < 10; k++ ) {
        for ( int i = 1; i < 1000; i++ ) {
            auto t = Transaction::createRandomSample( i, gen, ubyte );

            auto out = make_shared< vector< uint8_t > >();


            t->serializeInto( out, true );

            if ( _fail ) {
                corrupt_byte_vector( out, gen, ubyte );
            }

            if ( _fail ) {
                CATCH_REQUIRE_THROWS( Transaction::deserialize( out, 0, out->size(), true ) );
            } else {
                auto imp = Transaction::deserialize( out, 0, out->size(), true );
                CATCH_REQUIRE( imp != nullptr );
            }
        }
    }
}


void test_tx_list_serialize_deserialize( bool _fail ) {
    boost::random::mt19937 gen;

    boost::random::uniform_int_distribution<> ubyte( 0, 255 );

    for ( int k = 0; k < 10; k++ ) {
        for ( int i = 0; i < 30; i++ ) {
            auto t = TransactionList::createRandomSample( i, gen, ubyte );


            auto out = t->serialize( true );

            if ( _fail ) {
                corrupt_byte_vector( out, gen, ubyte );
            }


            CATCH_REQUIRE( out != nullptr );

            if ( _fail ) {
                CATCH_REQUIRE_THROWS( TransactionList::deserialize(
                    t->createTransactionSizesVector( true ), out, 0, true ) );
            } else {
                auto imp = TransactionList::deserialize(
                    t->createTransactionSizesVector( true ), out, 0, true );
                CATCH_REQUIRE( imp != nullptr );
            }
        }
    }
}


void test_committed_block_serialize_deserialize( bool _fail ) {
    boost::random::mt19937 gen;

    Schain chain;
    auto cryptoManager = make_shared< CryptoManager >( chain );
#ifdef BITE
    auto biteManager = make_shared< BiteManager >( chain );
#endif

    boost::random::uniform_int_distribution<> ubyte( 0, 255 );

    u256 stateRoot;

    for ( int k = 0; k < 100; k++ ) {
        for ( int i = 0; i < 20; i++ ) {
            auto t = CommittedBlock::createRandomSample( cryptoManager, i, gen, ubyte );

            auto out = t->serialize();

            if ( _fail ) {
                corrupt_byte_vector( out, gen, ubyte );
            }


            CATCH_REQUIRE( out != nullptr );

            if ( _fail ) {
                CATCH_REQUIRE_THROWS( CommittedBlock::deserialize( out, cryptoManager,
#ifdef BITE
                biteManager,
#endif
                false ));
            } else {
                ptr< CommittedBlock > imp = nullptr;

                try {
                    imp = CommittedBlock::deserialize( out, cryptoManager,
#ifdef BITE
                biteManager,
#endif
                    false );
                } catch ( ParsingException& e ) {
                    SkaleException::logNested( e, err );
                    throw( e );
                }
                CATCH_REQUIRE( imp != nullptr );
                CATCH_REQUIRE( imp->getStateRoot() == t->getStateRoot() );
            }
        }
    }
}

void test_committed_block_list_serialize_deserialize() {
    boost::random::mt19937 gen;

    Schain chain;
    auto cryptoManager = make_shared< CryptoManager >( chain );
#ifdef BITE
    auto biteManager = make_shared< BiteManager >( chain );
#endif


    boost::random::uniform_int_distribution<> ubyte( 0, 255 );

    for ( int k = 0; k < 5; k++ ) {
        for ( int i = 1; i < 50; i++ ) {
            auto t = CommittedBlockList::createRandomSample( cryptoManager, i, gen, ubyte );

            auto out = t->serialize();


            CATCH_REQUIRE( out != nullptr );


            ptr< CommittedBlockList > imp = nullptr;

            try {
                imp = CommittedBlockList::deserialize( cryptoManager,
#ifdef BITE
                biteManager,
#endif
                    t->createSizes(), out, 0 );
            } catch ( ParsingException& e ) {
                SkaleException::logNested( e, err );
                throw( e );
            }
            CATCH_REQUIRE( imp != nullptr );
        }
    }
}


CATCH_TEST_CASE( "Serialize/deserialize transaction", "[tx-serialize][unit][correctness]" ) {
    CATCH_SECTION( "Test successful serialize/deserialize" )


    test_tx_serialize_deserialize( false );

    CATCH_SECTION( "Test corrupt serialize/deserialize" )

    test_tx_serialize_deserialize( true );

    // Test successful serialize/deserialize failure
}

CATCH_TEST_CASE( "Serialize/deserialize transaction list", "[tx-list-serialize][unit][correctness]" ) {
    CATCH_SECTION( "Test successful serialize/deserialize" )


    test_tx_list_serialize_deserialize( false );

    CATCH_SECTION( "Test corrupt serialize/deserialize" )

    test_tx_list_serialize_deserialize( true );
}


CATCH_TEST_CASE( "Serialize/deserialize committed block", "[committed-block-serialize][end-to-end][correctness]" ) {
    CATCH_SECTION( "Test successful serialize/deserialize" )

    test_committed_block_serialize_deserialize( false );

    // CATCH_SECTION( "Test corrupt serialize/deserialize" )

    // test_committed_block_serialize_deserialize( true);

    // Test successful serialize/deserialize failure
}


CATCH_TEST_CASE( "Serialize/deserialize committed block list", "[committed-block-list-serialize][end-to-end][correctness]" ) {
    CATCH_SECTION( "Test successful serialize/deserialize" )

    test_committed_block_list_serialize_deserialize();

    // CATCH_SECTION( "Test corrupt serialize/deserialize" )

    // test_committed_block_serialize_deserialize( true);

    // Test successful serialize/deserialize failure
}

CATCH_TEST_CASE( "Test committed block fragment/defragment", "[committed-block-defragment][end-to-end][correctness]" ) {
    CATCH_SECTION( "Test successful serialize/deserialize" )

    test_committed_block_fragment_defragment( false );

    // CATCH_SECTION( "Test corrupt serialize/deserialize" )

    // test_committed_block_serialize_deserialize( true);

    // Test successful serialize/deserialize failure
}


class CryptoFixture {
public:
    CryptoFixture(){};

    ~CryptoFixture() {}
};
