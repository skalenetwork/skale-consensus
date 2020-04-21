#!/bin/bash
set -x
set -e
cd ../libBLS/deps;
./build.sh;
cd ..;
cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug -DBUILD_STATIC_LIBS=ON
cmake --build build  -- -j$(nproc)
cd ../cryptopp;
make;
cd ../jsoncpp
cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug -DBUILD_STATIC_LIBS=ON
cmake --build build  -- -j$(nproc)
cd ../curl;                                                                                                                                      
cmake . -Bbuild -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Debug -DCURL_INCLUDE_DIR=../curl/include -DCURL_LIBRARY=../curl/build/lib/libcurl-d.a -DBUILD_STATIC_LIBS=ON
cmake --build build  -- -j$(nproc)
cd ../libzmq;
cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug -DBUILD_STATIC_LIBS=ON
cmake --build build  -- -j$(nproc)
cd ../libjson-rpc-cpp
cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug -DJSONCPP_INCLUDE_DIR=../jsoncpp/include -DJSONCPP_LIBRARY=../jsoncpp/build/src/lib_json/libjsoncpp.a -DBUILD_STATIC_LIBS=ON -DCURL_INCLUDE_DIR=../curl/include -DCURL_LIBRARY=../curl/build/lib/libcurl-d.a 
cmake --build build  -- -j$(nproc)
