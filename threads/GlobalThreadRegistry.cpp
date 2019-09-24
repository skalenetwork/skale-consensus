//
// Created by kladko on 24.09.19.
//



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

    CHECK_STATE(!joined);

    allThreads.push_back(_t);
}