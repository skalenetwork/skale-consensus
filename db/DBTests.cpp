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

    @file DBTests.cpp
    @author Stan Kladko
    @date 2019
*/



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

    boost::random::uniform_int_distribution<> ubyte(0, 255);

    if (std::system(("rm -rf " + fileName).c_str()) != 0) {
        BOOST_THROW_EXCEPTION(runtime_error("Remove failed"));
    }

    auto db = make_shared<BlockDB>(fileName, node_id(1), 200);


    for (int i = 1; i < 200; i++) {
        auto t = CommittedBlock::createRandomSample(i, gen, ubyte);

        db->saveBlock(t, 200);

        auto bb = db->getBlock(t->getBlockID());

        REQUIRE(bb != nullptr);
    }

}


TEST_CASE("Save/read block", "[block-save-read-db]") {
    SECTION("Test successful save/read")


        test_committed_block_save();
}
