#!/bin/bash
set -x
set -e
cd ../cryptopp;
make;
cd ../libzmq;
cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug -DBUILD_STATIC_LIBS=ON
cmake --build build  -- -j$(nproc)
cd ../jsoncpp
cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug -DBUILD_STATIC_LIBS=ON
cmake --build build  -- -j$(nproc)
cp -rf include jsoncpp
cd ../curl
cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug -DBUILD_STATIC_LIBS=ON -DBUILD_SHARED_LIBS=OFF
cmake --build build  -- -j$(nproc)
cd ../libjson-rpc-cpp
cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug -DBUILD_STATIC_LIBS=ON -DCURL_LIBRARY=../curl/build/libcurl-d.a -DCURL_INCLUDE_DIR=../curl/include \
    -DJSONCPP_INCLUDE_DIR=../jsoncpp -DJSONCPP_LIBRARY=../jsoncpp/build/src/lib_json/libjsoncpp.a
cmake --build build  -- -j$(nproc) --trace
cd ../libBLS/deps;
./build.sh;
cd ..;
cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug -DBUILD_STATIC_LIBS=ON
cmake --build build  -- -j$(nproc)

