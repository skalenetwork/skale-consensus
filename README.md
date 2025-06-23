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


Read the spec for more exciting features https://github.com/skalenetwork/skale-consensus/blob/develop/docs/consensus-spec.md

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

## Running tests

After the build completes, the *build* directory contains a test binary named **consensust**,  
which can run a number of consensus tests.

The test subdirectories are located in the **tests** directory.  
To run a specific test, navigate to its corresponding subdirectory.

Examples:


To run one node

```
cd test/onenode
sudo NO_ULIMIT_CHECK=1 TEST_TIME_S=180 TEST_TRANSACTIONS_PER_BLOCK=10 ../../build/consensust [consensus-basic]  
```




## Running SGX simulation test

You can run four and 16 nodes test  using sgx simulation and 
test keys.

First install docker and docker compose, since the test runs sgxwallet in a docker container

```bash
sudo apt update
sudo apt install -y \
    ca-certificates \
    curl \
    gnupg \
    lsb-release
sudo mkdir -p /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | \
    sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
echo \
  "deb [arch=$(dpkg --print-architecture) \
  signed-by=/etc/apt/keyrings/docker.gpg] \
  https://download.docker.com/linux/ubuntu \
  $(lsb_release -cs) stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
  sudo apt update
  sudo apt install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin             
```

Now start sgx server in a docker container 

```bash
cd run_sgx_test
sudo ./run.bash
```

Check that sgxwallet has successfully started (it takes several minutes)

You can check progress by doing 

```bash
docker logs run_sgx_test-sgxwallet-1
```
When the sgxwallet is up and running, you will see the following message in the log

```
[2025-05-20 12:49:43.323] [info] Started ZMQ worker thread 14
[2025-05-20 12:49:43.323] [info] Starting ZMQ worker thread 15
[2025-05-20 12:49:43.323] [info] Started ZMQ worker thread 15
[2025-05-20 12:49:43.323] [info] Created thread pool
[2025-05-20 12:49:43.323] [info] Releasing SGX worker threads  ...
[2025-05-20 12:49:43.323] [info] Starting zmq server on port 1031 ...
Completed initAll.
[2025-05-20 12:49:43.323] [info] Released SGX worker threads.
[2025-05-20 12:49:43.323] [info] Inited zmq server.
Found test keys.
[2025-05-20 12:49:43.323] [info] ZMQ server socket created and bound.
[2025-05-20 12:49:43.323] [info] Started zmq read loop.
```


Now you can check that test keys are available by running the following command:

```bash
cd sgx_data
ls
```

You should see, in particular, the following files:

```
16node.json  
4node.json 
```

These files are config files for 4 and 16 nodes respectively used by the test.

Now you can run the test with 4 nodes or 16 nodes.

```
cd ../test/fournodes
sudo TEST_TIME_S=180 TEST_TRANSACTIONS_PER_BLOCK=10 ../../build/consensust [consensus-basic]
```

or

```
cd ../test/sixteennodes
sudo TEST_TIME_S=180 TEST_TRANSACTIONS_PER_BLOCK=10 ../../build/consensust [consensus-basic]
```


The test checks if the config files ```4node.json and 16node.json ``` are present, and runs the tests against the wallet. If the files are not present,
the test runs in a mockup mode. 



## Contributing

**If you have any questions please ask our development community on [Discord](https://discord.gg/vvUtWJB).**

[![Discord](https://img.shields.io/discord/534485763354787851.svg)](https://discord.gg/vvUtWJB)

## License

[![License](https://img.shields.io/github/license/skalenetwork/skale-consensus.svg)](LICENSE)

Copyright (C) 2018-present SKALE Labs
