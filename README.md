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

### Isolated Consensus Process Smoke Test

After reconfiguring and building the `consensust` target, run:

```bash
./build/consensust "[consensus-process]"
```

Building `consensust` also builds the `consensus_test_child` helper. The test launches a real
one-node chain in a separate process, checks committed-block progress and a
catch-up TCP response, then verifies graceful shutdown. Configuration, databases
and logs use a private temporary directory; listeners use a process-specific
Linux loopback address. Run this test serially within each test-runner process.
The parent uses one absolute deadline per control or catch-up exchange, including
all frame transfers. Progress polling passes its scenario deadline to each command;
shutdown acknowledgment and process exit share a single budget. A failed control
exchange closes the channel to prevent reuse of partial frames or late responses.
The harness shares typed command and event tags in `ConsensusControlProtocol.h`;
their string conversions preserve the wire format and reject unknown tags.
`ConsensusProcess` exposes readiness-on-construction, `progress(deadline)`, and
`stop(deadline)`; environment preparation and process supervision live in its
implementation file. Parent and child share `ControlChannel` for length-prefixed
commands/responses, frame validation, and failure closure. `SocketIO` provides
deadline-aware `sendAll()` / `receiveExact()` for both control messages and the
separate production catch-up protocol. The child retains its 120-second command
read deadline.
The child and the in-process `E2ETestHelper` share `startConsensusEngine()` in
`tests/e2e/harness/ConsensusStartup.h` for engine construction, patch configuration,
node creation, and bootstrap. Startup options also support external-face replay
metadata; a supplied external face must outlive the engine. Environment setup,
readiness waits, and shutdown remain with each caller.
Test sockets use the shared move-only `Socket` owner in `network/Socket.h`, also
used by production outgoing connections, server connections, and TCP listeners. Test-only
factories in `TestSockets.h` set close-on-exec; the spawn action
explicitly duplicates the child's control endpoint onto stdin.
The shared owner's destructor performs best-effort cleanup without logging;
explicit `reset()` returns a close error for caller-side reporting and preserves
`errno`. Production outgoing connections retain their existing nonzero-descriptor
requirement; the low-level owner itself supports descriptor zero.
Accepted sockets move directly into their server connection. Listener ownership
reads and explicit cleanup are mutex-protected, without holding the mutex during
blocking `accept()`. Normal node shutdown still wakes and joins accept threads
before listener destruction; explicit cleanup is not an accept-cancellation API.
The parent retains the child's exit status across shutdown and cleanup. Startup,
progress, and shutdown exceptions include the original error, exit code or
terminating signal, and diagnostic directory. Failure cleanup first checks for an
already-exited child; any supervisor-requested SIGKILL is labeled separately
from a natural exit or crash. A dedicated failure-child fixture covers these paths
without starting consensus.
The parent kills/reaps the child on failure. If startup or
shutdown fails, or the supervisor is destroyed during exception unwinding, it
preserves the private directory and prints its path to stderr; child logs are
in its `logs/` subdirectory. After a verified clean shutdown, normal destruction
removes the directory. The network scenario injects `EMFILE` only into the child's
catch-up listener using `TestHooks::TCPAcceptErrorHook`. It requires at least three
injected failures from the production retry loop, an accept-exhaustion warning, and
block progress while injection remains active. After removing the hook, it verifies
a fresh catch-up response, an acceptance-recovery log, further block progress, and
a stable injection counter. It does not consume descriptors or alter resource limits;
database operations remain unaffected. Normal consensus startup still requires a soft
descriptor limit of at least 65535. Run just this scenario with
`./build/consensust "[descriptor-exhaustion]"`. Connection floods and quantitative
CPU/backoff or log-rate assertions are not covered.
The network scenario lives in `tests/e2e/network/DescriptorExhaustionTests.cpp`;
supervisor, socket-ownership, and control-protocol tests remain in
`tests/e2e/ConsensusProcessTests.cpp`.
`TestHooks::TCPAcceptErrorHook` can inject a positive errno immediately before
`accept()` for a selected listener. Zero preserves real acceptance; injected errors
use the production error classification. Like the existing runtime test hooks, it
is compiled in but inactive by default. Callbacks run under the hook mutex and
must remain short and nonblocking. Component tests cover resource-error classification,
listener isolation, and real acceptance after scoped hook removal. This tests TCP
error handling, not whole-engine survival under real process-wide descriptor exhaustion.

`tests/unit/network/PendingConnectionQueueTests.cpp` tests the actual 1024-entry
pending TCP queue without starting consensus or worker threads. It verifies admission
at capacity, repeated rejection without retained connections, descriptor cleanup,
readmission after removing one entry, and cleanup of all queued connections on
destruction. Run it with `./build/consensust "[pending-queue]"`; it requires descriptor
headroom for 1024 queued sockets plus the test runner's own descriptors.

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
