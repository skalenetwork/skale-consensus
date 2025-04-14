# SKALE Consensus: a Blockchain Consensus engine in C++

[![Discord](https://img.shields.io/discord/534485763354787851.svg)](https://discord.gg/vvUtWJB)
![Build and test skale-consensus](https://github.com/skalenetwork/skale-consensus/workflows/Build%20and%20test%20skale-consensus/badge.svg)


**SKALE Consensus** is an ultra-high-performance blockchain consensus engine written in C++.

## Key Features of SKALE Consensus

- **Over 10,000 TPS**
- **Byzantine fault tolerant** — no block gaps, and stable performance even with up to 1/3 of nodes offline
- **Provably secure**
- **Forkless**
- **Single-block finality** — blocks are immediately finalized upon commitment
- **Resilient to arbitrarily long network disruptions and delays** through an asynchronous network model
- **Multiple block proposers per block** — ensures protocol stability even when some proposers are offline
- **Secure against MEV and front-running** — provably resistant to manipulation


Read the spec for more exciting features https://docs.skale.network/technology/consensus-spec 

See visualization of live conseneus https://www.youtube.com/watch?v=0NGCSRjjPkk


## Building from Source

The preferred build and execution environment is **Ubuntu 22.04**.

Later versions of Ubuntu may work, but they are not officially tested.

### 1. Install Dependencies

```bash
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install -yq libprocps-dev gcc-11 g++-11 valgrind gawk sed libffi-dev ccache \
    libgoogle-perftools-dev yasm texinfo autotools-dev automake \
    python3 python3-pip cmake libtool build-essential pkg-config autoconf wget \
    git libargtable2-dev libmicrohttpd-dev libhiredis-dev redis-server openssl \
    libssl-dev doxygen libgcrypt20-dev
```


2. Clone repo   

```bash
git clone --recurse-submodules https://github.com/skalenetwork/skale-consensus.git
```


3. Build dependencies in debug mode
```bash        
cd scripts && ./build.sh DEBUG=1
```         

4. Configure the CMake build in Debug Mode.

```   
cd .. && cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug
```

5  Build All Targets Using All Available CPU Code

```
cmake --build build -- -j$(nproc) 
```

### Running tests

After the build completes, *build* directory includes a test binary  **consensust** 
that can run number of consensus tests.

The test subdirectories are located in the **tests** directory.
To run a particular test, cd into its respective subdirectory.

Examples:


To run one node

```
cd test/onenode
sudo NO_ULIMIT_CHECK=1 TEST_TIME_S=180 TEST_TRANSACTIONS_PER_BLOCK=10 ../../build/consensust [consensus-basic]  
```


To run for nodes

```
cd test/fournodes
sudo NO_ULIMIT_CHECK=1 TEST_TIME_S=180 TEST_TRANSACTIONS_PER_BLOCK=10 ../../build/consensust [consensus-basic]  
```

## Contributing

**If you have any questions please ask our development community on [Discord](https://discord.gg/vvUtWJB).**

[![Discord](https://img.shields.io/discord/534485763354787851.svg)](https://discord.gg/vvUtWJB)

## License

[![License](https://img.shields.io/github/license/skalenetwork/skale-consensus.svg)](LICENSE)

Copyright (C) 2018-present SKALE Labs
