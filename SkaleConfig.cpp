
#include "SkaleConfig.h"
#include "Log.h"

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

    string name = prefix + _n.substr(0, 15);


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
    assert(ok == 0);
    return std::string(buf);
}

thread_local ptr<Log> logThreadLocal_ = nullptr;
