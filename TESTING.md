# SKALE Consensus Testing Guide

SKALE Consensus has three test workflows. All use the Catch-based `consensust`
executable; they differ in scope, engine lifecycle, configuration, and setup.

| Workflow | Purpose | Engine and configuration |
|---|---|---|
| Unit tests | Check individual components and helpers | No running consensus engine |
| Focused E2E tests | Check specific behavior using a real consensus engine | Engine started by the test, either in-process or in a harness-managed child |
| Configuration-driven consensus runs | Exercise full consensus with a selected node topology | Full engine run using configuration directories such as `test/fournodes` |

## Table of Contents

- [SKALE Consensus Testing Guide](#skale-consensus-testing-guide)
  - [Table of Contents](#table-of-contents)
  - [Test Organization Guidelines](#test-organization-guidelines)
      - [1. Component-Based Structure and Tagging](#1-component-based-structure-and-tagging)
      - [2. Test Type Tags](#2-test-type-tags)
      - [3. Scope Tags](#3-scope-tags)
  - [Running Tests](#running-tests)
  - [1. Unit Tests](#1-unit-tests)
  - [2. Focused E2E Tests](#2-focused-e2e-tests)
    - [Shared Harness and Its Own Tests](#shared-harness-and-its-own-tests)
  - [3. Configuration-Driven Consensus Runs](#3-configuration-driven-consensus-runs)
    - [Single Node Test](#single-node-test)
    - [Multi-Node Tests (Requires SGX Setup)](#multi-node-tests-requires-sgx-setup)
    - [Test Parameters](#test-parameters)
  - [SGX Simulation Testing](#sgx-simulation-testing)
    - [Prerequisites](#prerequisites)
    - [Starting SGX Wallet](#starting-sgx-wallet)
    - [Monitoring SGX Wallet Startup](#monitoring-sgx-wallet-startup)
    - [Verifying Test Keys](#verifying-test-keys)
  - [Contributing Test Cases](#contributing-test-cases)

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

Commands below assume an existing build in `build/`. Run unit and focused E2E
commands from the repository root; configuration-driven runs explicitly change
to the selected configuration directory.

Catch tag expressions select tests in the same executable for all three
workflows. Quote expressions to avoid shell interpretation. Component and concern
tags can further narrow a selection:

```bash
# Run all RLP correctness unit tests
./build/consensust '[rlp][unit][correctness]'

# Run all security RLP tests (any scope)
./build/consensust '[rlp][security]'

# Run all performance tests
./build/consensust '[performance]'
```

## 1. Unit Tests

Tests under `tests/unit/` check individual components without starting consensus.

```bash
./build/consensust '[unit]~[performance]'
./build/consensust '[unit][harness]'
```

## 2. Focused E2E Tests

These tests start a real consensus engine to inspect specific behavior, such as
restart/bootstrap, BITE patch transitions, or TCP accept-error recovery. There are
currently two execution styles:

- **In-process:** tests start and stop the engine inside `consensust`.
- **Harness-managed child:** `ConsensusProcess` launches a separate executable,
  waits for readiness, exchanges control messages, and manages shutdown and
  diagnostics.

```bash
# Real consensus in a harness-managed child
./build/consensust '[end-to-end][descriptor-exhaustion]'

# Focused in-process engine tests selected by the existing script
./build/consensust '[end-to-end][db]'

# A specific BITE in-process scenario (requires a BITE build)
./build/consensust '[end-to-end][reencryption-signature-transition]'
```

### Shared Harness and Its Own Tests

Shared test infrastructure lives in `tests/harness/`, separately from test cases.
It is grouped into `process/` for child supervision, `consensus/` for real engine
startup and the consensus child executable, and `transport/` for the control
protocol, channels, and socket helpers. Shared diagnostics and timeout policy
remain at the harness root.

## 3. Configuration-Driven Consensus Runs

These tests exercise full consensus with the topology selected by a configuration
directory under `test/`. Unlike focused E2E scenarios that choose their own setup,
the standard `[consensus-basic]` workflow is launched from that directory.

These are also Catch tests, defined in `tests/consensus_tests.cpp`. Multiple
configured nodes do not necessarily mean multiple OS processes: this workflow
creates the configured nodes through the engine in the test runner, rather than
automatically launching one child process per node.

### Single Node Test

```bash
cd test/onenode
sudo NO_ULIMIT_CHECK=1 TEST_TIME_S=180 TEST_TRANSACTIONS_PER_BLOCK=10 ../../build/consensust '[consensus-basic]'
```

### Multi-Node Tests (Requires SGX Setup)

For 4-node and 16-node tests, you need to set up SGX simulation first (see [SGX Simulation Testing](#sgx-simulation-testing)).

```bash
# 4-node test (from the repository root)
cd test/fournodes
sudo TEST_TIME_S=180 TEST_TRANSACTIONS_PER_BLOCK=10 ../../build/consensust '[consensus-basic]'
cd ../..

# 16-node test
cd test/sixteennodes
sudo TEST_TIME_S=180 TEST_TRANSACTIONS_PER_BLOCK=10 ../../build/consensust '[consensus-basic]'
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
