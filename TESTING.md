# SKALE Consensus Testing Guide

This document provides comprehensive testing instructions for SKALE Consensus, including unit tests, integration tests, and end-to-end testing scenarios.

## Table of Contents

- [Test Organization](#test-organization)
- [Running Tests](#running-tests)
- [End-to-end tests](#end-to-end-tests)
- [SGX Simulation Testing](#sgx-simulation-testing)

---

## Test Organization Guidelines

Tests in the SKALE Consensus codebase follow a **multi-dimensional tagging and directory structure** that promotes clarity, modularity, and scalability. The tagging system is structured along three main dimensions: **Component**, **Test Type**, and **Scope**. 

  
  
#### 1. Component-Based Structure and Tagging

**Directory Structure**  
Each unit test must reside in a directory named after the component it tests. For example:
- `rlp/` for RLP-related tests
- `crypto/` for cryptographic component tests
- `consensus/` for consensus algorithm logic
- `network/` for network-level functionality

**Tagging Rules**:
- All tests within a given directory must be tagged with the corresponding component tag, e.g., `[rlp]`, `[crypto]`.
- Tests targeting specific internal classes or modules should include a **sub-component tag**, typically matching the tested class or submodule name.

**Examples**:
- `[rlp][rlp-stream]` for tests in `rlp/RLPStreamTests.cpp`
- `[rlp][rlp-item]` for tests in `rlp/RLPItemTests.cpp`

#### 2. Test Type Tags

These tags describe the **purpose or concern** of the test:

- `[correctness]`: Ensures functional correctness and semantic compliance of the component under test.  
  _Example: Validating that RLP-encoded data decodes back to the original form._

- `[security]`: Verifies enforcement of **non-functional safety constraints** that prevent potential abuse or misuse.  
  _Example: Checking input size bounds, nesting depth limits, or overflow protections._

- `[performance]`: Benchmarks throughput, latency, or resource usage of specific operations.  
  _Example: Measuring cryptographic signing speed or RLP parsing time for large inputs._

> **Note:** Security tests focus on constraints that **do not affect correctness**, but are critical to system safety (e.g., limiting input data size or computational cost).


#### 3. Scope Tags

These tags define the **granularity** of the test:

- `[unit]`: Isolated testing of a single component or class, independent of external systems.
- `[integration]`: Tests the interaction between multiple components or subsystems.
- `[end-to-end]`: Tests the complete consensus pipeline or system behavior under realistic or simulated conditions.


---
## Running Tests

```bash
# Run all RLP correctness unit tests
./build/consensust [rlp][unit][correctness]

# Run all security RLP tests (any scope)
./build/consensust [rlp][security]

# Run all performance tests
./build/consensust [performance]

# Run all non-performance tests
./build/consensust ~[performance]
```

---

## End-to-End Tests

End-to-end tests simulate real network conditions with multiple nodes.

### Single Node Test

```bash
cd test/onenode
sudo NO_ULIMIT_CHECK=1 TEST_TIME_S=180 TEST_TRANSACTIONS_PER_BLOCK=10 ../../build/consensust [consensus-basic]
```

### Multi-Node Tests (Requires SGX Setup)

For 4-node and 16-node tests, you need to set up SGX simulation first (see [SGX Simulation Testing](#sgx-simulation-testing)).

```bash
# 4-node test
cd test/fournodes
sudo TEST_TIME_S=180 TEST_TRANSACTIONS_PER_BLOCK=10 ../../build/consensust [consensus-basic]

# 16-node test
cd test/sixteennodes
sudo TEST_TIME_S=180 TEST_TRANSACTIONS_PER_BLOCK=10 ../../build/consensust [consensus-basic]
```

### Test Parameters

- `TEST_TIME_S` - Test duration in seconds
- `TEST_TRANSACTIONS_PER_BLOCK` - Number of transactions per block
- `NO_ULIMIT_CHECK=1` - Bypass ulimit checks (for single-node tests)

## SGX Simulation Testing

Multi-node tests require SGX wallet simulation using Docker.

### Prerequisites

Install Docker and Docker Compose:

```bash
sudo apt update
sudo apt install -y ca-certificates curl gnupg lsb-release

# Add Docker's official GPG key
sudo mkdir -p /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | \
    sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg

# Set up the repository
echo \
  "deb [arch=$(dpkg --print-architecture) \
  signed-by=/etc/apt/keyrings/docker.gpg] \
  https://download.docker.com/linux/ubuntu \
  $(lsb_release -cs) stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

# Install Docker
sudo apt update
sudo apt install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
```

### Starting SGX Wallet

```bash
cd run_sgx_test
sudo ./run.bash
```

### Monitoring SGX Wallet Startup

The SGX wallet takes several minutes to initialize. Monitor progress:

```bash
docker logs run_sgx_test-sgxwallet-1
```

**Ready when you see:**
```
[2025-05-20 12:49:43.323] [info] Started zmq read loop.
Found test keys.
```

### Verifying Test Keys

Check that configuration files are available:

```bash
cd sgx_data
ls
```

You should see:
- `4node.json` - Configuration for 4-node tests
- `16node.json` - Configuration for 16-node tests


## Contributing Test Cases

When adding new tests:

1. **Use appropriate tags** following the component/type/scope convention
2. **Place tests in correct directories** matching the component tag
3. **Include both positive and negative test cases**
4. **Add performance tests for critical paths**
5. **Document any special setup requirements**

For more information on contributing, see the main [README.md](README.md#contributing).
