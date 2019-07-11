//
// Created by kladko on 7/11/19.
//

#include "../SkaleCommon.h"
#include "ImportedTransaction.h"
#include "PendingTransaction.h"


#define BOOST_PENDING_INTEGER_LOG2_HPP
#include <boost/integer/integer_log2.hpp>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>


#include "../thirdparty/catch.hpp"

#include "Transaction.h"



void test_tx_serialize_deserialize(bool _fail) {


    boost::random::mt19937 gen;

    boost::random::uniform_int_distribution<> ubyte( 0, 255 );

    for ( int k = 0; k < 10; k++ ) {
        for ( int i = 1; i < 1000; i++ ) {
            auto sample = make_shared< vector< uint8_t > >( i, 0 );


            for ( uint32_t j = 0; j < sample->size(); j++ ) {
                sample->at( j ) = ubyte( gen );
            }


            PendingTransaction t( sample );

            auto out = make_shared< vector< uint8_t > >();

            t.serializeInto( out, true );

            if (_fail) {

                int random = ubyte(gen) % out->size();
                auto first = out->at(random);
                out->at(random) = first + 1;
            }

            if (_fail) {
                REQUIRE_THROWS( ImportedTransaction::deserialize( out, 0, out->size() ) );
            } else {

                auto imp = ImportedTransaction::deserialize( out, 0, out->size());
                REQUIRE( imp != nullptr );
            }
        }
    }
}

TEST_CASE( "Serialize/deserialize transaction", "[transaction-serialize]" )


{



    SECTION("Test successful serialize/deserialize")


    test_tx_serialize_deserialize( false );

    SECTION("Test corrupt serialize/deserialize")

    test_tx_serialize_deserialize( true );

    // Test successful serialize/deserialize failure

}


