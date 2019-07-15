//
// Created by kladko on 7/11/19.
//


#include "../SkaleCommon.h"
#include "../exceptions/ParsingException.h"

#include "../datastructures/CommittedBlock.h"

#define BOOST_PENDING_INTEGER_LOG2_HPP
#include <boost/integer/integer_log2.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>


#include "../thirdparty/catch.hpp"

#include "BlockDB.h"


void test_committed_block_save() {
    static string fileName = "/tmp/test_committed_block_save";
    boost::random::mt19937 gen;

    boost::random::uniform_int_distribution<> ubyte( 0, 255 );

    std::system(("rm -rf " + fileName).c_str());

    auto db = make_shared< BlockDB >(  fileName, node_id( 1 ) );


    for ( int i = 1; i < 200; i++ ) {
        auto t = CommittedBlock::createRandomSample( i, gen, ubyte );

        db->saveBlock( t );

        auto serializedBlock = db->getSerializedBlock( t->getBlockID() );

        auto bb = CommittedBlock::deserialize( serializedBlock );

        REQUIRE( bb != nullptr );
    }

}


TEST_CASE( "Save/read block", "[block-save-read-db]" )


{
    SECTION( "Test successful save/read" )


    test_committed_block_save();
}
