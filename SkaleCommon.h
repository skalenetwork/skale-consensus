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

    @file SkaleCommon.h
    @author Stan Kladko
    @date 2018
*/

#ifndef _SKALE_COMMON_H
#define _SKALE_COMMON_H

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
#include <shared_mutex>
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

#include <iostream>


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
#include <boost/multiprecision/cpp_int.hpp>

#include <boost/crc.hpp>

class ConsensusEngine;
class SkaleLog;


#define BOOST_STRONG_TYPEDEF(T, D)                                                     \
    struct D : boost::totally_ordered1< D,                                               \
                   boost::totally_ordered2< D, T,                                        \
                       boost::multipliable2< D, T,                                       \
                           boost::addable2< D, T, boost::subtractable2< D, T >>>> > { \
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

static const uint64_t LEVELDB_SHARDS = 4;

static const uint64_t BLOCK_PROPOSAL_HISTORY_SIZE = 1;

static const uint64_t COMMITTED_TRANSACTIONS_HISTORY = 1024 * 1024;

static const uint64_t MAX_ACTIVE_CONSENSUSES = 5;

static const uint64_t MAX_CONSENSUS_HISTORY  = 2 * MAX_ACTIVE_CONSENSUSES;

static const uint64_t SESSION_KEY_CACHE_SIZE  = 2;
static const uint64_t SESSION_PUBLIC_KEY_CACHE_SIZE  = 16;

static constexpr uint64_t MAX_CATCHUP_DOWNLOAD_BYTES = 16 * 1024 * 1024;

static constexpr uint64_t MAX_TRANSACTIONS_PER_BLOCK = 8 * 1024;

static constexpr int64_t EMPTY_BLOCK_INTERVAL_MS = 3000;

static constexpr uint64_t MIN_BLOCK_INTERVAL_MS = 1;

static  constexpr uint64_t PROPOSAL_RETRY_INTERVAL_MS = 500;

static constexpr uint64_t CATCHUP_INTERVAL_MS = 5000;

static constexpr uint64_t MONITORING_INTERVAL_MS = 1000;

static constexpr uint64_t STUCK_MONITORING_INTERVAL_MS = 3000;

static constexpr uint64_t STUCK_RESTART_INTERVAL_MS = 1080000; // 18 min

static constexpr uint64_t WAIT_AFTER_NETWORK_ERROR_MS = 3000;

static constexpr uint64_t CONNECTION_REFUSED_LOG_INTERVAL_MS = 10 * 60 * 1000;

// Non-tunable params

static constexpr uint32_t SOCKET_BACKLOG = 64;

static constexpr size_t HASH_LEN = 32;

static constexpr size_t PARTIAL_HASH_LEN = 8;

static constexpr uint32_t SLOW_TEST_INITIAL_GENERATE = 0;
// static constexpr uint32_t SLOW_TEST_INITIAL_GENERATE  = 10000;
static constexpr uint64_t SLOW_TEST_MESSAGE_INTERVAL = 10000;

static constexpr size_t MAX_HEADER_SIZE = 8 * MAX_TRANSACTIONS_PER_BLOCK;

static const int MODERN_TIME = 1547640182;

static const int MAX_BUFFER_SIZE = 8 * 1024 * 1024;

static constexpr uint64_t MAGIC_NUMBER = 0x1396A22050B30;

static constexpr uint64_t TEST_MAGIC_NUMBER = 0x2456032650150;

static constexpr uint64_t  MAX_DEFERRED_QUEUE_SIZE_FOR_BLOCK = 1024;

static const uint64_t KNOWN_TRANSACTIONS_HISTORY = 2 * MAX_TRANSACTIONS_PER_BLOCK;
static const uint64_t MAX_KNOWN_TRANSACTIONS_TOTAL_SIZE = 256 * 1024 * 1024; // 256 MBYTE FOR NOW

static const uint64_t  KNOWN_MSG_HASHES_SIZE = 1024;

static const uint64_t LEVELDB_STATS_HISTORY = 8;

static const int ZMQ_TIMEOUT = 1000;

static const int CONSENSUS_ZMQ_HWM = 32;

static const int NODE_DEATH_INTERVAL_MS = 30000;


enum port_type {
    PROPOSAL = 0, CATCHUP = 1, RETRIEVE = 2, HTTP_JSON = 3, BINARY_CONSENSUS = 4, ZMQ_BROADCAST = 5,
    MTA = 6,
    STATUS = 10
};


template<typename T>
using ptr = typename std::shared_ptr<T>;  // #define ptr shared_ptr
template<typename T>
static inline std::string tstr(T x) {
    return std::to_string(x);
}  // #define tstr(x) to_string(x)

using fs_path = boost::filesystem::path;  // #define fs_path boost::filesystem::path


typedef array<uint8_t, PARTIAL_HASH_LEN> partial_sha_hash;

class SkaleCommon {
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

BOOST_STRONG_TYPEDEF(uint64_t, fragment_index);

BOOST_STRONG_TYPEDEF(uint64_t, te_share_index);

BOOST_STRONG_TYPEDEF(int, file_descriptor);

BOOST_STRONG_TYPEDEF(char, out_buffer);

BOOST_STRONG_TYPEDEF(char, in_buffer);

using u256 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<256, 256, boost::multiprecision::unsigned_magnitude,
boost::multiprecision::unchecked, void>>;


inline std::string className(const std::string &prettyFunction) {
    size_t colons = prettyFunction.find("::");
    if (colons == std::string::npos)
        return "::";
    size_t begin = prettyFunction.substr(0, colons).rfind(" ") + 1;
    size_t end = colons - begin;

    return prettyFunction.substr(begin, end);
}


static const num_threads NUM_SCHAIN_THREADS = num_threads(1);


static const num_threads NUM_DISPATCH_THREADS = num_threads(1);

static const num_threads NUM_ORACLE_THREADS = num_threads(1);

static const uint64_t  ORACLE_QUEUE_TIMEOUT_MS = 1000;
static const uint64_t  ORACLE_TIMEOUT_MS = 20000;

static const uint64_t DEFAULT_DB_STORAGE_LIMIT = 5000000000; // 5Gbyte

static const uint64_t  MAX_DELAYED_MESSAGE_SENDS = 128;

static const uint64_t  MAX_PROPOSAL_QUEUE_SIZE = 4;

static const uint64_t SGX_SSL_PORT = 1026;

static const uint64_t BLOCK_PROPOSAL_RECEIVE_TIMEOUT_MS = 30000;

static const uint64_t REBROADCAST_TIMEOUT_MS = 120000;

static const uint64_t ZMQ_RECEIVE_RETRY_MS = 10;

static const uint64_t DEFAULT_MIN_PRICE = 100000;

static const uint64_t COMMON_COIN_ROUND = 4;

static const uint64_t  ORACLE_RECEIPTS_MAP_SIZE = 100000;

static const uint64_t  ORACLE_FUTURE_JITTER_MS = 500;

static const uint64_t TE_MAGIC_SIZE = 10;

static const uint64_t AES_KEY_LEN_BYTES = 16;

extern void setThreadName(std::string const &_n, ConsensusEngine* _engine);

extern std::string getThreadName();

#define CHECK_ARGUMENT(_EXPRESSION_) \
    if (!(_EXPRESSION_)) { \
        auto __msg__ = string("Argument Check failed:") + #_EXPRESSION_ + "\n" + __CLASS_NAME__ + ":" + __FUNCTION__ +  \
        + " " + string(__FILE__) + ":" + to_string(__LINE__); \
        throw InvalidArgumentException(__msg__, __CLASS_NAME__);}

#define CHECK_STATE(_EXPRESSION_) \
    if (!(_EXPRESSION_)) { \
        auto __msg__ = string("State check failed::") + #_EXPRESSION_ +  " " + string(__FILE__) + ":" + to_string(__LINE__); \
        throw InvalidStateException(__msg__, __CLASS_NAME__);}

#define ORACLE_CHECK_STATE(_EXPRESSION_) \
    if (!(_EXPRESSION_)) { \
        auto __msg__ = string("Oracle check failed: ") + #_EXPRESSION_; \
        throw InvalidStateException(__msg__, "");}


#define CHECK_ARGUMENT2(_EXPRESSION_, _MSG_) \
    if (!(_EXPRESSION_)) { \
        auto __msg__ = string("Check failed::") + #_EXPRESSION_ +  " " + string(__FILE__) + ":" + to_string(__LINE__); \
        throw InvalidArgumentException(__msg__ + ":" + _MSG_, __CLASS_NAME__);}

#define CHECK_STATE2(_EXPRESSION_, _MSG_) \
    if (!(_EXPRESSION_)) { \
        auto __msg__ = string("Check failed::") + #_EXPRESSION_ +  " " + string(__FILE__) + ":" + to_string(__LINE__); \
        throw InvalidStateException(__msg__ + ":" + _MSG_, __CLASS_NAME__);}

#define ORACLE_CHECK_STATE2(_EXPRESSION_, _MSG_) \
    if (!(_EXPRESSION_)) { \
        auto __msg__ = string("Oracle check failed: ") + _MSG_; \
        throw InvalidStateException(__msg__, ""); }

#define INJECT_TEST(__TEST_NAME__, __TEST_CODE__) \
 { static bool __TEST_NAME__ = (getenv(#__TEST_NAME__) != nullptr); \
 if (__TEST_NAME__) {__TEST_CODE__ ;} };

#define LOCK(_M_) lock_guard<recursive_mutex> _lock_(_M_);

#define RETURN_IF_PREVIOUSLY_CALLED(__BOOL__) \
    auto __previouslyCalled = __BOOL__.exchange(true); \
    if (__previouslyCalled) { return;}

#endif // SKALE_COMMON_H
