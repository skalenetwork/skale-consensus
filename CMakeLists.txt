cmake_minimum_required(VERSION 3.5)


add_subdirectory(libBLS)


include("cmake/HunterGate.cmake")
include("cmake/precompiledheader.cmake")




HunterGate( URL "https://github.com/ruslo/hunter/archive/v0.23.76.tar.gz" SHA1 "c7b60993e841850e2c449afd454f5d5aa4ec04e4" )



project(consensus)

# zeromq

hunter_add_package(ZeroMQ)

find_package(ZeroMQ CONFIG REQUIRED)

#leveldb

hunter_add_package(leveldb)
find_package(leveldb CONFIG REQUIRED)

#folly
hunter_add_package(folly)
find_package(folly CONFIG REQUIRED)




#set (CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-Wreturn-type -Winit-self")

if( SKALED_LATEST_STANDARD )
    set( CMAKE_CXX_STANDARD 17 )
    set( LIB_NAME_cryptopp "cryptopp" )
else()
    set( CMAKE_CXX_STANDARD 17 )
    hunter_add_package(cryptopp)
    add_compile_options(-Wall -Wextra -Werror)
    find_package( cryptopp CONFIG REQUIRED )
    set( LIB_NAME_cryptopp "cryptopp-static" )
endif()

add_definitions("-DZMQ_BUILD_DRAFT_API")

add_definitions("-DZMQ_EXPERIMENTAL")
add_definitions("-DZMQ_NONBLOCKING")

#add_definitions(-DGOOGLE_PROFILE) // uncomment to profile


if(CMAKE_PROJECT_NAME STREQUAL "consensus" )
    # to install compiler cache:  sudo apt-get install ccache
    find_program(CCACHE_FOUND ccache)
    if(CCACHE_FOUND)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    endif(CCACHE_FOUND)
    add_definitions("-DCONSENSUS_STANDALONE")


endif()




#Threads package

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#Boost package
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)

hunter_add_package(Boost COMPONENTS filesystem system thread log)
find_package(Boost CONFIG REQUIRED filesystem system thread log)

hunter_add_package(spdlog)
find_package(spdlog)

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
AUX_SOURCE_DIRECTORY(pendingqueue pendingqueue_src)
AUX_SOURCE_DIRECTORY(protocols/binconsensus protocols_binconsensus_src)
AUX_SOURCE_DIRECTORY(protocols/blockconsensus protocols_blockconsensus_src)
AUX_SOURCE_DIRECTORY(protocols protocols_src)
AUX_SOURCE_DIRECTORY(thirdparty thirdparty_src)
AUX_SOURCE_DIRECTORY(threads threads_src)
AUX_SOURCE_DIRECTORY(json json_src)
AUX_SOURCE_DIRECTORY(exceptions exceptions_src)
AUX_SOURCE_DIRECTORY(headers headers_src)



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
        ${pendingqueue_src}
        ${protocols_src}
        ${protocols_binconsensus_src}
        ${protocols_blockconsensus_src}
        ${thirdparty_src}
        ${threads_src}
        ${json_src}
        ${exceptions_src}
        Agent.h Agent.cpp SkaleConfig.h SkaleConfig.cpp Log.h Log.cpp microprofile.cpp miniz.c)



include_directories(${BLS_INCLUDE_DIRS})
link_directories( ${BLS_WITH_FF_INCLUDE_DIRS} )

SET(LINK_LIBRARIES Threads::Threads Boost::log Boost::thread Boost::system Boost::filesystem ${LIB_NAME_cryptopp}
        ZeroMQ::libzmq-static leveldb::leveldb bls gcov Folly::Folly)




# consensus library

add_library(consensus ${SRC_FILES})

target_link_libraries(consensus ${LINK_LIBRARIES})
add_precompiled_header(consensus SkaleConfig.h FORCE_INCLUDE)
add_dependencies(consensus bls gmp secp256k1)

# consensus test agent

add_executable(consensustest ConsensusTest.h ConsensusTest.cpp)

# libgoogle-perftools-dev
if(CMAKE_PROJECT_NAME STREQUAL "consensus" )

#  sudo apt-get install -qq -yy libgoogle-perftools-dev

    target_link_libraries(consensustest consensus tcmalloc)
else()
    target_link_libraries(consensustest consensus)
endif()


