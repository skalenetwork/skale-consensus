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

    @file GlobalThreadRegistry.cpp
    @author Stan Kladko
    @date 2019
*/


#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "GlobalThreadRegistry.h"


vector<thread*> GlobalThreadRegistry::allThreads;

recursive_mutex GlobalThreadRegistry::mutex;
bool GlobalThreadRegistry::joined = false;

void GlobalThreadRegistry::joinAll() {

    if (joined)
        return;

    lock_guard<recursive_mutex> lock(mutex);

    joined = true;

    for (auto &&thread : GlobalThreadRegistry::allThreads) {
        thread->join();
        ASSERT(!thread->joinable());
    }

}

void GlobalThreadRegistry::add(thread* _t) {

    CHECK_ARGUMENT(_t);

    lock_guard<recursive_mutex> lock(mutex);

    allThreads.push_back(_t);
}