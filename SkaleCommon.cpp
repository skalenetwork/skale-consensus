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

    @file SkaleCommon.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"

#include "Log.h"
#include "exceptions/FatalError.h"

#include "thirdparty/json.hpp"
#include "crypto/SHAHash.h"
#include "node/Node.h"



// TODO Fix microiprofile linkage
#if MICROPROFILE_ENABLED
    extern "C" void MicroProfileOnThreadCreate( const char* );
#endif

void setThreadName( std::string const& _n ) {

    string prefix;

    if (Node::nodeIDs.size() > 1) {
        prefix = to_string(logThreadLocal_->getNodeID());
    } else {
        prefix = "";
    }

    string name = (prefix + _n).substr(0, 15);


#if defined( __GLIBC__ )
    pthread_setname_np( pthread_self(), name.c_str() );
#elif defined( __APPLE__ )
    pthread_setname_np(name.c_str() );
#else
#error "error: setThreadName: we're not in Linux nor in apple?!"
#endif
#if MICROPROFILE_ENABLED
    MicroProfileOnThreadCreate( _n.c_str() );
#endif
}

std::string getThreadName(){
    char buf[32];
    int ok = pthread_getname_np(pthread_self(), buf, sizeof(buf));
    ASSERT(ok == 0);
    return std::string(buf);
}

thread_local ptr<Log> logThreadLocal_ = nullptr;
