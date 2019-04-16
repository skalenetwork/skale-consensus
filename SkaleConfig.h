#ifndef _CONFIG_H
#define _CONFIG_H

#include <network.h>
#include <assert.h>
#include <sys/time.h>
#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <sstream>
#include <thread>
#include <mutex>
#include <iostream>
#include "assert.h"
#include <condition_variable>
#include "stdlib.h"
#include <unistd.h>
#include <string>
#include <cstring>
#include <queue>
#include <vector>
#include <array>
#include <set>
#include <signal.h>
#include <atomic>
#include <exception>
#include <files.h>
#include <arpa/inet.h>
#include <cassert>
#include <boost/assert.hpp>
#include <sys/socket.h>
#include <sys/types.h>
#include <chrono>
#include <array>
#include <algorithm>
#include <iterator>
#include "cryptlib.h"
#include "sha3.h"
#include "sha.h"
#include <fstream>
#include <netinet/in.h>
#include <netdb.h>


#include <boost/exception/exception.hpp>
#include <boost/exception/info.hpp>
#include <boost/exception/info_tuple.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/throw_exception.hpp>

#include "boost/filesystem.hpp"
#include <boost/filesystem/operations.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/detail/assert.hpp>

#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>


class Log;


#define BOOST_STRONG_TYPEDEF(T, D)                                                     \
    struct D : boost::totally_ordered1< D,                                               \
                   boost::totally_ordered2< D, T,                                        \
                       boost::multipliable2< D, T,                                       \
                           boost::addable2< D, T, boost::subtractable2< D, T > > > > > { \
        T t;                                                                             \
        D( const T& t_ ) : t( t_ ){};                                                    \
        D( T&& t_ ) : t( std::move( t_ ) ){};                                            \
        D() = default;                                                                   \
        D( const D& t_ ) = default;                                                      \
        D( D&& ) = default;                                                              \
        D& operator=( const D& rhs ) = default;                                          \
        D& operator=( D&& ) = default;                                                   \
        explicit operator T&() { return t; }                                             \
        explicit operator const T&() const { return t; }                                 \
        bool operator==( const D& rhs ) const { return t == rhs.t; }                     \
        bool operator<( const D& rhs ) const { return t < rhs.t; }                       \
                                                                                         \
        D& operator+=( const T& rhs ) {                                                  \
            t += rhs;                                                                    \
            return *this;                                                                \
        }                                                                                \
        D& operator-=( const T& rhs ) {                                                  \
            t -= rhs;                                                                    \
            return *this;                                                                \
        }                                                                                \
        D& operator*=( const T& rhs ) {                                                  \
            t *= rhs;                                                                    \
            return *this;                                                                \
        }                                                                                \
                                                                                         \
        explicit operator bool() const { return t != 0; }                                \
                                                                                         \
        friend const char* operator+( const char* s, const D& d ) { return s + d.t; }    \
        operator std::string() const { return to_string( t ); }                          \
        friend std::string to_string( const D& d ) { return to_string( d.t ); }          \
        friend std::istream& operator>>( std::istream& s, D& d ) {                       \
            s >> d.t;                                                                    \
            return s;                                                                    \
        }                                                                                \
        friend std::ostream& operator<<( std::ostream& s, const D& d ) {                 \
            s << d.t;                                                                    \
            return s;                                                                    \
        }                                                                                \
    }


using namespace std;

static const uint64_t BLOCK_PROPOSAL_HISTORY_SIZE = 1;

static const uint64_t COMMITTED_TRANSACTIONS_HISTORY = 1000000;

static constexpr uint64_t MAX_CATCHUP_DOWNLOAD_BYTES = 1000000000;


static constexpr uint64_t MAX_TRANSACTIONS_PER_BLOCK = 10000;

static constexpr int64_t EMPTY_BLOCK_INTERVAL_MS = 10000;

static constexpr uint64_t MIN_BLOCK_INTERVAL_MS = 1;

static constexpr uint64_t COMMITTED_BLOCK_STORAGE_SIZE = 1;

static constexpr uint64_t CATCHUP_INTERVAL_MS = 10000;

static constexpr uint64_t WAIT_AFTER_NETWORK_ERROR_MS = 3000;



// Non-tunable params

static constexpr uint32_t SOCKET_BACKLOG = 64;

static constexpr size_t SHA3_HASH_LEN = 32;

static constexpr size_t PARTIAL_SHA_HASH_LEN = 8;

static constexpr uint32_t SLOW_TEST_INITIAL_GENERATE = 0;
// static constexpr uint32_t SLOW_TEST_INITIAL_GENERATE  = 10000;
static constexpr uint64_t SLOW_TEST_MESSAGE_INTERVAL = 10000;

static constexpr size_t MAX_HEADER_SIZE = 8 * MAX_TRANSACTIONS_PER_BLOCK;

static const int MODERN_TIME = 1547640182;

static const int MAX_BUFFER_SIZE = 10000000;

static constexpr uint32_t CONSENSUS_MESSAGE_LEN = 73;

static constexpr uint64_t MAGIC_NUMBER = 0x1396A22050B30;

static constexpr uint64_t TEST_MAGIC_NUMBER = 0x2456032650150;





static const uint64_t KNOWN_TRANSACTIONS_HISTORY = 2 * MAX_TRANSACTIONS_PER_BLOCK;


enum port_type {
    PROPOSAL = 0, CATCHUP = 1, RETRIEVE = 2, HTTP_JSON = 3, BINARY_CONSENSUS = 4, ZMQ_BROADCAST = 5,
    MTA = 6
};


template<typename T>
using ptr = typename std::shared_ptr<T>;  // #define ptr shared_ptr
template<typename T>
static inline std::string tstr(T x) {
    return std::to_string(x);
}  // #define tstr(x) to_string(x)

using fs_path = boost::filesystem::path;  // #define fs_path boost::filesystem::path


typedef array<uint8_t, PARTIAL_SHA_HASH_LEN> partial_sha_hash;

class SkaleConfig {
public:

    static constexpr const char *NODE_FILE_NAME = "Node.json";
    static constexpr const char *SCHAIN_DIR_NAME = "schains";
};

enum BinaryDecision {
    DECISION_UNDECIDED, DECISION_TRUE, DECISION_FALSE
};

BOOST_STRONG_TYPEDEF(uint64_t, block_id);

BOOST_STRONG_TYPEDEF(uint64_t, node_count);

BOOST_STRONG_TYPEDEF(uint64_t, msg_id);

BOOST_STRONG_TYPEDEF(uint64_t, node_id);

BOOST_STRONG_TYPEDEF(uint64_t, schain_id);


BOOST_STRONG_TYPEDEF(unsigned int, tcp_connection);

BOOST_STRONG_TYPEDEF(uint64_t, instance_id);

BOOST_STRONG_TYPEDEF(uint64_t, schain_index);

BOOST_STRONG_TYPEDEF(uint64_t, bulk_data_len);

BOOST_STRONG_TYPEDEF(uint64_t, bin_consensus_round);

BOOST_STRONG_TYPEDEF(uint8_t, bin_consensus_value);


BOOST_STRONG_TYPEDEF(uint64_t, transaction_count);

BOOST_STRONG_TYPEDEF(uint16_t, network_port);

BOOST_STRONG_TYPEDEF(uint64_t, msg_len);

BOOST_STRONG_TYPEDEF(uint64_t, msg_nonce);

BOOST_STRONG_TYPEDEF(uint64_t, num_threads);

BOOST_STRONG_TYPEDEF(int, file_descriptor);

BOOST_STRONG_TYPEDEF(char, out_buffer);

BOOST_STRONG_TYPEDEF(char, in_buffer);


inline std::string className(const std::string &prettyFunction) {
    size_t colons = prettyFunction.find("::");
    if (colons == std::string::npos)
        return "::";
    size_t begin = prettyFunction.substr(0, colons).rfind(" ") + 1;
    size_t end = colons - begin;

    return prettyFunction.substr(begin, end);
}


//typedef map<schain_index, ptr<SHA3Hash> > proposed_block_hashes;

static const num_threads NUM_SCHAIN_THREADS = num_threads(1);


static const num_threads NUM_DISPATCH_THREADS = num_threads(1);




inline void printPartialHash(ptr<partial_sha_hash> hash) {
    for (size_t i = 0; i < PARTIAL_SHA_HASH_LEN; i++) {
        cerr << to_string((*hash)[i]);
    }
}

extern void setThreadName(std::string const &_n);

extern std::string getThreadName();


extern thread_local ptr<Log> logThreadLocal_;

#define ASSERT(_EXPRESSION_) \
    if (!(_EXPRESSION_)) { \
        auto msg = "Assert failed:: " + string(__FILE__) + ":" + to_string(__LINE__); \
        cerr << msg << endl; \
        assert((_EXPRESSION_)); \
    }


#define ASSERT2(_EXPRESSION_, _MSG_) \
    if (!((_EXPRESSION_))) { \
        auto msg = "Assert failed:: " + string(_MSG_) + ":" + string(__FILE__) + ":" + to_string(__LINE__); \
        cerr << msg << endl; \
        assert((_EXPRESSION_)); \
    }
#endif
