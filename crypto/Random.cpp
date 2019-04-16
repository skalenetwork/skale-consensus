//
// Created by stan on 19.02.18.
//



#include <stdlib.h>
#include <mutex>


using namespace std;

#include "Random.h"

uint64_t Random::randomProtocolID() {
    return static_cast<uint64_t>(rand());
}

