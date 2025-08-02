# SKALE Consensus: a Blockchain Consensus engine in C++

[![Discord](https://img.shields.io/discord/534485763354787851.svg)](https://discord.gg/vvUtWJB)
![Build and test skale-consensus](https://github.com/skalenetwork/skale-consensus/workflows/Build%20and%20test%20skale-consensus/badge.svg)


SKALE ABBA Consensus is a high-performance, provably secure, EVM-compatible protocol that achieves single-block finality. It powers the SKL Gas-Free Network and the FAIR MEV-Free Network. Built on asynchronous Byzantine Binary Agreement (ABBA), BLS signatures, and threshold encryption, the implementation is available on GitHub.      

# Key Features 

- **High Throughput**: Supports over **10,000 transactions per second (TPS)**, enabling scalable decentralized applications.

- **Byzantine Fault Tolerance (BFT)**: Maintains consensus and liveness with up to **1/3 of validator nodes offline**, with **no block gaps** or degraded performance.

- **Provable Security**: Backed by **formal proofs** ensuring the **integrity** and **correctness** of the protocol under adversarial conditions.

- **Forkless Operation**: Consensus guarantees a **single canonical chain**, eliminating chain splits and reducing complexity.

- **Instant Finality**: Implements **single-block finality**, where each committed block is **immediately finalized**, eliminating the need for confirmations.

- **Asynchronous Network Resilience**: Remains **safe and live** under **arbitrarily long network delays or partitions**, using an **asynchronous consensus model**.

- **Redundant Block Proposers**: **Multiple proposers** participate in each round, ensuring **liveness and stability** even if some proposers are offline or faulty.

- **MEV and Front-Running Resistance**: **Provably secure** against **Maximal Extractable Value (MEV)** attacks and **front-running**, ensuring **fair transaction ordering** and **user protection**.


Read the spec for more exciting features [https://docs.skale.network/technology/consensus-spec ](https://github.com/skalenetwork/skale-consensus/blob/develop/docs/consensus-spec.md)

See visualization of live conseneus https://www.youtube.com/watch?v=0NGCSRjjPkk


## Building from Source

The preferred build and execution environment is **Ubuntu 22.04**.

Later versions of Ubuntu may work, but they are not officially tested.

### 1. Install packages

```bash
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install -yq libprocps-dev gcc-11 g++-11 valgrind gawk sed libffi-dev ccache \
    libgoogle-perftools-dev yasm texinfo autotools-dev automake \
    python3 python3-pip cmake libtool build-essential pkg-config autoconf wget \
    git libargtable2-dev libmicrohttpd-dev libhiredis-dev redis-server openssl \
    libssl-dev doxygen libgcrypt20-dev
```


### 2. Clone the repo   

```bash
git clone --recurse-submodules https://github.com/skalenetwork/skale-consensus.git
```


###  3. Build dependencies in debug mode
```bash        
cd skale-consensus/deps && ./build.sh DEBUG=1
```         

###  4. Configure the CMake build in Debug M    ode.

```   
cd .. && cmake . -Bbuild -DCMAKE_BUILD_TYPE=Debug
```

###  5.  Build All Targets Using All Available CPU Code

```
cmake --build build -- -j$(nproc) 
```

---

## Testing

SKALE Consensus includes comprehensive test suites covering unit tests, integration tests, and end-to-end scenarios.

### Quick Test Run

After building, you can run a basic test:

```bash
cd test/onenode
sudo NO_ULIMIT_CHECK=1 TEST_TIME_S=60 TEST_TRANSACTIONS_PER_BLOCK=10 ../../build/consensust [consensus-basic]
```

### Running Specific Tests

Tests are organized with a multi-dimensional tagging system:

```bash
# Run RLP unit tests that only test correctness (ignores safety and performance tests)
./build/consensust [rlp][unit][correctness]

# Run all crypto tests
./build/consensust [crypto]

# Run performance tests
./build/consensust [performance]
```

### Multi-Node Testing

For more detailed testing scenarios, including 4-node and 16-node tests with SGX simulation, see our comprehensive **[Testing Guide](TESTING.md)**.

The testing guide covers:
- Detailed test organization and tagging
- SGX simulation setup with Docker
- Performance testing procedures



## Contributing

**If you have any questions please ask our development community on [Discord](https://discord.gg/vvUtWJB).**

[![Discord](https://img.shields.io/discord/534485763354787851.svg)](https://discord.gg/vvUtWJB)

## License

[![License](https://img.shields.io/github/license/skalenetwork/skale-consensus.svg)](LICENSE)

Copyright (C) 2018-present SKALE Labs
