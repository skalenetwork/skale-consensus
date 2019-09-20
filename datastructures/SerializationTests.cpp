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


#include "../SkaleCommon.h"
#include "../exceptions/ParsingException.h"
#include "CommittedBlock.h"
#include "CommittedBlockList.h"


#include "Transaction.h"
#include "TransactionList.h"

#include "CommittedBlockFragment.h"
#include "CommittedBlockFragmentList.h"


#define BOOST_PENDING_INTEGER_LOG2_HPP

#include <boost/integer/integer_log2.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>


#include "../thirdparty/catch.hpp"

#include "Transaction.h"


void corrupt_byte_vector(ptr<vector<uint8_t> > _in, boost::random::mt19937 &_gen,
                         boost::random::uniform_int_distribution<> &_ubyte) {
    int randomPosition = _ubyte(_gen) % _in->size();
    auto b = _in->at(randomPosition);
    _in->at(randomPosition) = b + 1;
}


void test_committed_block_fragment_defragment(bool _fail) {
    boost::random::mt19937 gen;

    boost::random::uniform_int_distribution<> ubyte(0, 255);

    Log::init();


    for (int i = 1; i < 200; i++) {
        auto t = CommittedBlock::createRandomSample(i, gen, ubyte, i);

        auto list = make_shared<CommittedBlockFragmentList>(i, i);

        for (int j = 1; j <= i; j++) {
            list->addFragment(t->getFragment(i, j));
        }


        REQUIRE(list->isComplete());


        //auto out = t->getSerialized();

        if (_fail) {
            //corrupt_byte_vector( out, gen, ubyte );
        }


        //REQUIRE( out != nullptr );



        if (_fail) {
            REQUIRE_THROWS(CommittedBlock::defragment(list));
        } else {
            ptr<CommittedBlock> imp = nullptr;

            try {
                imp = CommittedBlock::defragment(list);
            } catch (Exception &e) {
                Exception::logNested(e, err);
                throw (e);
            }
            REQUIRE(imp != nullptr);
        }

    }


}


void test_tx_serialize_deserialize(bool _fail) {
    boost::random::mt19937 gen;

    boost::random::uniform_int_distribution<> ubyte(0, 255);

    for (int k = 0; k < 10; k++) {
        for (int i = 1; i < 1000; i++) {
            auto t = Transaction::createRandomSample(i, gen, ubyte);

            auto out = make_shared<vector<uint8_t> >();


            t->serializeInto(out, true);

            if (_fail) {
                corrupt_byte_vector(out, gen, ubyte);
            }

            if (_fail) {
                REQUIRE_THROWS(Transaction::deserialize(out, 0, out->size(), true));
            } else {
                auto imp = Transaction::deserialize(out, 0, out->size(), true);
                REQUIRE(imp != nullptr);
            }
        }
    }
}


void test_tx_list_serialize_deserialize(bool _fail) {
    boost::random::mt19937 gen;

    boost::random::uniform_int_distribution<> ubyte(0, 255);

    for (int k = 0; k < 10; k++) {
        for (int i = 0; i < 30; i++) {
            auto t = TransactionList::createRandomSample(i, gen, ubyte);


            auto out = t->serialize(true);

            if (_fail) {
                corrupt_byte_vector(out, gen, ubyte);
            }


            REQUIRE(out != nullptr);

            if (_fail) {
                REQUIRE_THROWS(TransactionList::deserialize(
                        t->createTransactionSizesVector(true), out, 0, true));
            } else {
                auto imp = TransactionList::deserialize(
                        t->createTransactionSizesVector(true), out, 0, true);
                REQUIRE(imp != nullptr);
            }
        }
    }
}


void test_committed_block_serialize_deserialize(bool _fail) {
    boost::random::mt19937 gen;

    boost::random::uniform_int_distribution<> ubyte(0, 255);

    for (int k = 0; k < 100; k++) {
        for (int i = 0; i < 20; i++) {
            auto t = CommittedBlock::createRandomSample(i, gen, ubyte);

            auto out = t->getSerialized();

            if (_fail) {
                corrupt_byte_vector(out, gen, ubyte);
            }


            REQUIRE(out != nullptr);

            if (_fail) {
                REQUIRE_THROWS(CommittedBlock::deserialize(out));
            } else {
                ptr<CommittedBlock> imp = nullptr;

                try {
                    imp = CommittedBlock::deserialize(out);
                } catch (ParsingException &e) {
                    Exception::logNested(e, err);
                    throw (e);
                }
                REQUIRE(imp != nullptr);
            }
        }
    }
}

void test_committed_block_list_serialize_deserialize() {
    boost::random::mt19937 gen;

    boost::random::uniform_int_distribution<> ubyte(0, 255);

    for (int k = 0; k < 5; k++) {
        for (int i = 1; i < 50; i++) {
            auto t = CommittedBlockList::createRandomSample(i, gen, ubyte);

            auto out = t->serialize();


            REQUIRE(out != nullptr);


            ptr<CommittedBlockList> imp = nullptr;

            try {
                imp = CommittedBlockList::deserialize(t->createSizes(), out, 0);
            } catch (ParsingException &e) {
                Exception::logNested(e, err);
                throw (e);
            }
            REQUIRE(imp != nullptr);
        }
    }
}


TEST_CASE("Serialize/deserialize transaction", "[tx-serialize]") {
    SECTION("Test successful serialize/deserialize")


        test_tx_serialize_deserialize(false);

    SECTION("Test corrupt serialize/deserialize")

        test_tx_serialize_deserialize(true);

    // Test successful serialize/deserialize failure
}

TEST_CASE("Serialize/deserialize transaction list", "[tx-list-serialize]") {
    SECTION("Test successful serialize/deserialize")


        test_tx_list_serialize_deserialize(false);

    SECTION("Test corrupt serialize/deserialize")

        test_tx_list_serialize_deserialize(true);

}


TEST_CASE("Serialize/deserialize committed block", "[committed-block-serialize]") {
    SECTION("Test successful serialize/deserialize")

        test_committed_block_serialize_deserialize(false);

    // SECTION( "Test corrupt serialize/deserialize" )

    // test_committed_block_serialize_deserialize( true);

    // Test successful serialize/deserialize failure
}


TEST_CASE("Serialize/deserialize committed block list", "[committed-block-list-serialize]") {
    SECTION("Test successful serialize/deserialize")

        test_committed_block_list_serialize_deserialize();

    // SECTION( "Test corrupt serialize/deserialize" )

    // test_committed_block_serialize_deserialize( true);

    // Test successful serialize/deserialize failure
}

TEST_CASE("Test committed block fragment/defragment", "[committed-block-defragment]") {
    SECTION("Test successful serialize/deserialize")

        test_committed_block_fragment_defragment(false);

    // SECTION( "Test corrupt serialize/deserialize" )

    // test_committed_block_serialize_deserialize( true);

    // Test successful serialize/deserialize failure
}
