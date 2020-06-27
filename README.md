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

## Installation Requirements

SKALE consensus has been built and tested on Ubuntu with Cmake 3.10.

<details>
    <summary>Click to expand:</summary>

Install Cmake:

```bash
wget https://cmake.org/files/v3.10/cmake-3.10.0-Linux-x86_64.sh && \
    chmod +x cmake-3.10.0-Linux-x86_64.sh && \
    ./cmake-3.10.0-Linux-x86_64.sh --skip-license --include-subdir && \
    sudo ln -sf `pwd`/cmake-3.10.0-Linux-x86_64/bin/* /usr/local/bin
```

Ensure that the required packages are installed by executing:

```bash
sudo apt-get update
sudo apt-get install -yq software-properties-common apt-utils libprocps-dev \
        g++-7 valgrind gawk sed libffi-dev ccache libgoogle-perftools-dev flex \
        bison yasm texinfo \
        autotools-dev autogen automake autoconf m4 shtool pkg-config sed gawk yasm nasm \
        python python-pip libtool build-essential
```

</details>

### Building from source on Ubuntu (Development)

Clone project and configure build:

```bash
git clone --recurse-submodules https://github.com/skalenetwork/skale-consensus.git
# Configure the project and create a build directory.
cd scripts && ./build.sh && cd ..   # build dependencies
cd scripts && ./build.py Debug      # build consensus in Debug mode
```

### Running tests

```bash
cd scripts && ./tests.py
```

Refer to the [docs](docs/) folder for more information.

## Libraries

-   [libBLS](https://github.com/skalenetwork/libBLS/)

## Contributing

**If you have any questions please ask our development community on [Discord](https://discord.gg/vvUtWJB).**

[![Discord](https://img.shields.io/discord/534485763354787851.svg)](https://discord.gg/vvUtWJB)

## License

[![License](https://img.shields.io/github/license/skalenetwork/skale-consensus.svg)](LICENSE)

Copyright (C) 2018-present SKALE Labs
