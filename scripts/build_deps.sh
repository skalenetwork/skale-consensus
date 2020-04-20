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
cd ../libzmq;
cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug -DBUILD_STATIC_LIBS=ON
cmake --build build  -- -j$(nproc)
cd ../libjson-rpc-cpp
cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug -DBUILD_STATIC_LIBS=ON
cmake --build build  -- -j$(nproc)
