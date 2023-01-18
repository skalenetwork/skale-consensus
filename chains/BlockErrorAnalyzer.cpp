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

    @file TestConfig.cpp
    @author Stan Kladko
    @date 2019
*/

#include "SkaleCommon.h"
#include "Log.h"

#include "datastructures/CommittedBlock.h"
#include "BlockErrorAnalyzer.h"


void BlockErrorAnalyzer::analyze( ptr< CommittedBlock > _block ) {
    // for now just print block up to 100 chars
    auto serializedBlock = _block->serialize();
    auto bytesToPrint = serializedBlock->size() > 100 ? 100 : serializedBlock->size();
    LOG(err, string("Error in block:").append(string((const char*) serializedBlock->data(),
        bytesToPrint)));
}
BlockErrorAnalyzer::BlockErrorAnalyzer() {}
