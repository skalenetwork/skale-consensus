# SKALE Consensus: a BFT Consensus engine in C++

[![Discord](https://img.shields.io/discord/534485763354787851.svg)](https://discord.gg/vvUtWJB)
![Build and test skale-consensus](https://github.com/skalenetwork/skale-consensus/workflows/Build%20and%20test%20skale-consensus/badge.svg)


Skale-consensus  is an implementation of SKALE provable consensus spec as described here https://docs.skale.network/technology/consensus-spec

Key features of of SKALE consensus

* provably secure 
* forkless
* single block finality - the block becomes immediately finalized once committed.
* uses asynchronous network model and survives under arbitrarily long network distruptions and delays
* multiple block proposers provide protocol stability even if some block proposers are down

Read the spec for more exciting features. 

The consensus is under active improvement and research.


## Installation Requirements

SKALE consensus has been built and tested on Ubuntu 18.04 and later.

The preferred build and execution environment is currenty Ubuntu 22.04.  

Ensure that the required packages are installed by executing:

```bash
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install -yq libprocps-dev gcc-9 g++-9 valgrind gawk sed libffi-dev ccache \
    libgoogle-perftools-dev flex bison yasm texinfo autotools-dev automake \
    python3 python3-pip cmake libtool build-essential pkg-config autoconf wget \
    git  libargtable2-dev libmicrohttpd-dev libhiredis-dev redis-server openssl \
    libssl-dev doxygen libgcrypt20-dev
```

### Building from source on Ubuntu (Development)

Steps to build from source:

```bash
# clone repo
git clone --recurse-submodules https://github.com/skalenetwork/skale-consensus.git
# build dependencies
cd scripts && ./build_deps.sh 
 # Configure the Cmake build.
cd .. && cmake . -Bbuild
# now run hunter bug workaround
mkdir -p "${HOME}"/.hunter/_Base/Download/crc32c/1.0.5/dc7fa8c/ && wget -O "${HOME}"/.hunter/_Base/Download/crc32c/1.0.5/dc7fa8c/hunter-1.0.5.tar.gz https://github.com/hunter-packages/crc32c/archive/refs/tags/hunter-1.0.5.tar.gz
#  now build all targets using all available CPU cores
cmake --build build -- -j$(nproc) 
```

### Running tests

Navigate to the testing directories and run `./consensusd .`

## Libraries

-   [libBLS](https://github.com/skalenetwork/libBLS) by [SKALE Labs](https://skalelabs.com/)


## An important note about production readiness:

The SKALE consensus is still in active development and contains bugs. This software should be regarded as _alpha software_. Development is still subject to competing the specification, security hardening, further testing, and breaking changes.  **This consensus engine has not yet been reviewed or audited for security.** Please see [SECURITY.md](SECURITY.md) for reporting policies.


## Contributing

**If you have any questions please ask our development community on [Discord](https://discord.gg/vvUtWJB).**

[![Discord](https://img.shields.io/discord/534485763354787851.svg)](https://discord.gg/vvUtWJB)

## License

[![License](https://img.shields.io/github/license/skalenetwork/skale-consensus.svg)](LICENSE)

Copyright (C) 2018-present SKALE Labs
