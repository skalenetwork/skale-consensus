# SKALE Consensus: a BFT Consensus engine in C++

[![Discord](https://img.shields.io/discord/534485763354787851.svg)](https://discord.gg/vvUtWJB)
![Build and test skale-consensus](https://github.com/skalenetwork/skale-consensus/workflows/Build%20and%20test%20skale-consensus/badge.svg)

SKALE consensus utilizes multiple block proposers.  Block proposers distribute proposals to nodes and collect a BLS-signature based data availability proofs. An Asynchronous Binary Byzantine Agreement is then executed for each block proposal to reach consensus on whether it is data-available.  If multiple block proposals are known to be data-available, a BLS-based common coin is used to select the winning proposal that is committed to the chain.

SKALE Consensus uses an Asynchronous Binary Byzantine Agreement (ABBA) protocol. The current implementation uses ABBA from Mostefaoui _et al._ In general, any ABBA protocol can be used so long as it has the following properties:

-   Network model: protocol assumes asynchronous network messaging model.
-   Byzantine nodes: protocol assumes less than 1/3 of nodes are Byzantine.
-   Initial vote: protocol assumes each node makes an initial _yes_ or _no_ vote.
-   Consensus vote: protocol terminates with consensus vote of either _yes_ or _no_. Where consensus vote is _yes_, it is guaranteed that at least one honest node voted _yes_.

## An important note about production readiness:

The SKALE consensus is still in active development and contains bugs. This software should be regarded as _alpha software_. Development is still subject to competing the specification, security hardening, further testing, and breaking changes.  **This consensus engine has not yet been reviewed or audited for security.** Please see [SECURITY.md](SECURITY.md) for reporting policies.

## Roadmap

_to be posted soon_

## Installation Requirements

SKALE consensus has been built and tested on Ubuntu.

Ensure that the required packages are installed by executing:

```bash
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install -yq libprocps-dev gcc-9 g++-9 valgrind gawk sed libffi-dev ccache \
    libgoogle-perftools-dev flex bison yasm texinfo autotools-dev automake \
    python python3-pip cmake libtool build-essential pkg-config autoconf wget \
    git  libargtable2-dev libmicrohttpd-dev libhiredis-dev redis-server openssl \
    libssl-dev doxygen
```

### Building from source on Ubuntu (Development)

Clone project and configure build:

```bash
git clone --recurse-submodules https://github.com/skalenetwork/skale-consensus.git
# Configure the project and create a build directory.
cd scripts; ./build_deps.sh # build dependencies
cd ..; cmake . -Bbuild # Configure the build.
cmake --build build -- -j$(nproc) # Build all default targets using all cores.
```

### Running tests

Navigate to the testing directories and run `./consensusd .`

## Libraries

-   [libBLS by SKALE Labs](https://skalelabs.com/)

## Contributing

**If you have any questions please ask our development community on [Discord](https://discord.gg/vvUtWJB).**

[![Discord](https://img.shields.io/discord/534485763354787851.svg)](https://discord.gg/vvUtWJB)

## License

[![License](https://img.shields.io/github/license/skalenetwork/skale-consensus.svg)](LICENSE)

Copyright (C) 2018-present SKALE Labs
