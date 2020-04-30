#!/bin/bash
set -x
set -e
./build_deps.sh;
cd ..;
cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug -DBUILD_STATIC_LIBS=ON
cmake --build build  -- -j$(nproc)
