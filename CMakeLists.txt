cmake_minimum_required(VERSION 3.10)


option(COVERAGE "Build with code coverage support" OFF)
if (COVERAGE)
    message(STATUS "**********************")
    message(STATUS "*** COVERAGE is ON ***")
    message(STATUS "**********************")
    add_compile_options(-g --coverage)
    #
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    #
    set(CMAKE_SHARED_LINKER_FLAGS "--coverage ${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "--coverage ${CMAKE_EXE_LINKER_FLAGS}")
endif ()

if (NOT DEFINED DEPS_INSTALL_ROOT)
    set(DEPS_SOURCES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/libBLS/deps")
    set(DEPS_INSTALL_ROOT "${DEPS_SOURCES_ROOT}/deps_inst/x86_or_x64")
endif ()
message(INFO "---- DEPS_INSTALL_ROOT in consensus is: ${DEPS_INSTALL_ROOT}")

include("cmake/HunterGate.cmake")
include("cmake/precompiledheader.cmake")


HunterGate(URL "https://github.com/ruslo/hunter/archive/v0.23.76.tar.gz" SHA1 "c7b60993e841850e2c449afd454f5d5aa4ec04e4")

option( CONSENSUS_PROFILING "Build for profiling" OFF )
if( SKALED_PROFILING )
    set( CONSENSUS_PROFILING ON )
endif()
if( CONSENSUS_PROFILING )
    set( CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -pg" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg" )
    set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg" )
    set( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg" )
endif()

if( CMAKE_BUILD_TYPE STREQUAL "Release" )
    set( CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -O3" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3" )
elseif( CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" )
    set( CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -O3" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3" )
elseif( CMAKE_BUILD_TYPE STREQUAL "MinSizeRel" )
    set( CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -Os" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os" )
elseif( CMAKE_BUILD_TYPE STREQUAL "Debug" )
    set( CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -O0" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0" )
else()
	message( INFO "---- CMAKE_BUILD_TYPE = ${DEPS_INSTALL_ROOT} is not supported explicitly" )
endif()

add_subdirectory(libBLS)

project(consensus)

include_directories("${DEPS_INSTALL_ROOT}/include" "${DEPS_INSTALL_ROOT}/include/cryptopp"
        "cppzmq")
link_directories("${DEPS_INSTALL_ROOT}/lib")
set(CMAKE_PREFIX_PATH "${DEPS_INSTALL_ROOT}")

# zeromq


#find_package(оч CONFIG REQUIRED)


#leveldb

hunter_add_package(leveldb)
find_package(leveldb CONFIG REQUIRED)


#set (CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-Wreturn-type -Winit-self")


set(CMAKE_CXX_STANDARD 17)
execute_process( COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/gcc_ver_query.sh OUTPUT_VARIABLE GXX_MAJOR_VERSION )
message( STATUS "-------- GXX_MAJOR_VERSION is ${GXX_MAJOR_VERSION}" )
add_compile_options( -Wno-error=deprecated-declarations -Wno-error=return-type -Wno-error=unused-result -Wno-error=unused-variable -Wno-error=maybe-uninitialized -Wno-error=deprecated-copy -Wno-error=pessimizing-move -Wno-sign-compare)
if( CMAKE_BUILD_TYPE STREQUAL "Debug" )
    add_compile_options( -Wall -Wextra -Werror )
endif()
add_definitions("-DZMQ_BUILD_DRAFT_API")
add_definitions("-DUSER_SPACE")
add_definitions("-DZMQ_EXPERIMENTAL")
add_definitions("-DZMQ_NONBLOCKING")


#add_definitions(-DGOOGLE_PROFILE) // uncomment to profile


if (CMAKE_PROJECT_NAME STREQUAL "consensus")
    unset(SKALE_HAVE_BOOST_FROM_HUNTER)
    # to install compiler cache:  sudo apt-get install ccache
    find_program(CCACHE_FOUND ccache)
    if (CCACHE_FOUND)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    endif (CCACHE_FOUND)
    add_definitions("-DCONSENSUS_STANDALONE")
    add_definitions("-DMICROPROFILE_ENABLED=0")
endif ()


#Threads package

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#Boost package
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)


#Source dirs

AUX_SOURCE_DIRECTORY(. root_src)
AUX_SOURCE_DIRECTORY(abstracttcpserver abstractcpserver_src)
AUX_SOURCE_DIRECTORY(abstracttcpclient abstractcpclient_src)
AUX_SOURCE_DIRECTORY(blockproposal/pusher blockproposalpusher_src)
AUX_SOURCE_DIRECTORY(blockfinalize/client blockfinalizeclient_src)
AUX_SOURCE_DIRECTORY(blockfinalize/received blockfinalizereceived_src)
AUX_SOURCE_DIRECTORY(blockproposal/server blockproposalserver_src)
AUX_SOURCE_DIRECTORY(blockproposal/received blockproposalreceived_src)
AUX_SOURCE_DIRECTORY(blockproposal blockproposal_src)
AUX_SOURCE_DIRECTORY(catchup/client catchup_client_src)
AUX_SOURCE_DIRECTORY(catchup/server catchup_server_src)
AUX_SOURCE_DIRECTORY(chains chains_src)
AUX_SOURCE_DIRECTORY(crypto crypto_src)
AUX_SOURCE_DIRECTORY(datastructures datastructures_src)
AUX_SOURCE_DIRECTORY(db db_src)
AUX_SOURCE_DIRECTORY(log log_src)
AUX_SOURCE_DIRECTORY(messages messages_src)
AUX_SOURCE_DIRECTORY(network network_src)
AUX_SOURCE_DIRECTORY(node node_src)
AUX_SOURCE_DIRECTORY(pricing pricing_src)
AUX_SOURCE_DIRECTORY(pendingqueue pendingqueue_src)
AUX_SOURCE_DIRECTORY(protocols/binconsensus protocols_binconsensus_src)
AUX_SOURCE_DIRECTORY(protocols/blockconsensus protocols_blockconsensus_src)
AUX_SOURCE_DIRECTORY(protocols protocols_src)
AUX_SOURCE_DIRECTORY(thirdparty thirdparty_src)
AUX_SOURCE_DIRECTORY(threads threads_src)
AUX_SOURCE_DIRECTORY(json json_src)
AUX_SOURCE_DIRECTORY(exceptions exceptions_src)
AUX_SOURCE_DIRECTORY(headers headers_src)
AUX_SOURCE_DIRECTORY(monitoring monitoring_src)
AUX_SOURCE_DIRECTORY(utils utils_src)
AUX_SOURCE_DIRECTORY(statusserver statusserver_src)
AUX_SOURCE_DIRECTORY(sgxclient sgxclient_src)
AUX_SOURCE_DIRECTORY(oracle oracle_src)



SET(SRC_FILES
        ${abstractpushagent_src}
        ${abstractcpserver_src}
        ${abstractcpclient_src}
        ${blockproposalreceived_src}
        ${blockproposalserver_src}
        ${blockproposalpusher_src}
        ${blockfinalizeclient_src}
        ${blockfinalizereceived_src}
        ${blockretriever_src}
        ${blockproposal_src}
        ${catchup_client_src}
        ${catchup_server_src}
        ${chains_src}
        ${crypto_src}
        ${datastructures_src}
        ${db_src}
        ${headers_src}
        ${log_src}
        ${messages_src}
        ${network_src}
        ${node_src}
        ${oracle_src}
        ${pendingqueue_src}
        ${pricing_src}
        ${protocols_src}
        ${protocols_binconsensus_src}
        ${protocols_blockconsensus_src}
        ${thirdparty_src}
        ${threads_src}
        ${json_src}
        ${exceptions_src}
        ${monitoring_src}
        ${utils_src}
        ${statusserver_src}
        ${sgxclient_src}
        Agent.h Agent.cpp SkaleCommon.cpp Log.cpp microprofile.cpp miniz.c
        oracle/OracleClient.cpp oracle/OracleClient.h)


include_directories(. spdlog/include ${BLS_INCLUDE_DIRS} ${DEPS_INSTALL_ROOT}/include libBLS/bls libBLS
        "${DEPS_INSTALL_ROOT}/include" ${CMAKE_BINARY_DIR}/deps/include)


if (SKALE_HAVE_BOOST_FROM_HUNTER)
    set(BOOST_LIBS_4_CONSENSUS
            Boost::log Boost::thread Boost::system Boost::filesystem Boost::program_options
            )
else ()
    set(BOOST_LIBS_4_CONSENSUS
            #boost_log
            boost_thread boost_system boost_filesystem boost_program_options
            )
endif ()

set(DASH_D_SUFFIX "")
if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    set(DASH_SUFFIX "-d")
endif ()


SET(LINK_LIBRARIES
        backtrace
        Threads::Threads
        ${BOOST_LIBS_4_CONSENSUS}
        ${LIB_NAME_cryptopp}
        leveldb::leveldb
        "${DEPS_INSTALL_ROOT}/lib/libjsonrpccpp-client.a"
        "${DEPS_INSTALL_ROOT}/lib/libjsonrpccpp-server.a"
        "${DEPS_INSTALL_ROOT}/lib/libmicrohttpd.a"
        "${DEPS_INSTALL_ROOT}/lib/libjsonrpccpp-stub.a"
        "${DEPS_INSTALL_ROOT}/lib/libjsonrpccpp-common.a"
        bls # "${CMAKE_CURRENT_SOURCE_DIR}/libBLS/build/libbls.a"
        "${DEPS_INSTALL_ROOT}/lib/libzmq.a"
        "${DEPS_INSTALL_ROOT}/lib/libsodium.a"
        ff
        "${DEPS_INSTALL_ROOT}/lib/libcryptopp.a"
        gmp
        gcov
        gnutls
        gcrypt
        "${DEPS_INSTALL_ROOT}/lib/libcurl${DASH_D_SUFFIX}.a"
        "${DEPS_INSTALL_ROOT}/lib/libjsoncpp.a"
        "${DEPS_INSTALL_ROOT}/lib/libssl.a"
        "${DEPS_INSTALL_ROOT}/lib/libcrypto.a"
        dl
        z
        pthread
        idn2
        blake3
        )


# consensus library

add_library(consensus STATIC ${SRC_FILES})

target_compile_options( consensus PRIVATE -Wno-error=unused-variable )

target_link_libraries(consensus ${LINK_LIBRARIES})

# consensus test agent

add_executable(consensusd Consensusd.h Consensusd.cpp)

target_compile_options( consensusd PRIVATE -Wno-error=unused-variable )

# # libgoogle-perftools-dev
# if (CMAKE_PROJECT_NAME STREQUAL "consensus")
#     include_directories( ${DEPS_INSTALL_ROOT}/include )
#     #  sudo apt-get install -qq -yy libgoogle-perftools-dev
#     target_link_libraries(consensusd  consensus tcmalloc)
# else ()
target_link_libraries(consensusd consensus)
# endif ()

add_executable(consensust Consensust.h Consensust.cpp datastructures/SerializationTests.cpp db/DBTests.cpp)

target_compile_options( consensust PRIVATE -Wno-error=unused-variable )

# # libgoogle-perftools-dev
# if (CMAKE_PROJECT_NAME STREQUAL "consensus")
#     #  sudo apt-get install -qq -yy libgoogle-perftools-dev
#     target_link_libraries(consensust consensus tcmalloc)
# else ()
target_link_libraries(consensust consensus)
# endif ()
