## Code Review Focus

When reviewing code in pull requests:

1. Logic Correctness
   - First, ensure the changes are logically correct, complete, and satisfy the intended functionality.
   - Identify incorrect behaviors, missing edge cases, or violations of expected input/output.

2. Code Quality and Guidelines
   - Enforce the following standards across structure, naming, formatting, safety, and performance.

## Code Guidelines

### General Style

- In code files, line length must not exceed 100 characters. For documentation and other file types, follow their respective conventions.
- Function length must be ≤ 100 lines.
- All comments and code must be written in English with correct spelling.
- Comments must be professional and clearly explain the code’s logic or behavior.
- Use a consistent naming style (e.g., avoid mixing `snake_case` with `camelCase` in the same file).
- Names must be descriptive and searchable (e.g., `userAccountBalance`, not `x` or `data`).

### Function Best Practices

- Each function must perform only one task.
- Each function must have:
  - A clear name or a preceding comment describing what it does.
  - Precondition checks: validate all inputs.
  - Postcondition checks: ensure valid outputs after execution.
- Avoid nesting functions deeply or making them hard to refactor.

### Variables and Fields

- All global or class fields must:
  - Be well-named or
  - Have a preceding comment explaining their purpose.
- Avoid one-letter variable names, except for common counters (`i`, `j`).

### Error Handling

- Never ignore errors (e.g., discarded return values).
- All error cases must be handled explicitly using conditionals or exceptions.
- Use exceptions only for exceptional, non-recoverable cases.
- Do not use exceptions for normal control flow.
- Prefer exceptions over `assert` for error handling.

### Safety & Concurrency

- Avoid raw pointers unless necessary.
  - If raw pointers are used, ensure null checks before dereferencing.
- All shared data structures accessed concurrently must be:
  - Protected with mutexes, atomics, or other synchronization primitives.
- Avoid race conditions or data races by enforcing thread safety consistently.

## Performance Guidelines

### Memory and Copies

- Use `const` and `const references` where applicable to avoid unnecessary copies.
- Prefer `std::move` when ownership is transferred.
- Avoid expensive operations (e.g., heap allocations) inside tight loops or hot paths.
- Cache repeated values or function results locally if reused.

### Resource Management

- Use RAII principles for managing resources and memory.
- Prefer smart pointers (`std::unique_ptr`, `std::shared_ptr`) over raw pointers.
- Ensure deterministic cleanup via destructors or scoped guards.

### Logging and Observability

- Do not spam logs. Logging at `info` level should be concise.
- Avoid using `std::cout` or other raw output — use standard logging libraries.
- Avoid color formatting in logs (e.g., ANSI codes) — logs must remain plaintext for ElasticSearch and similar tools.

## Code Hygiene

- No commented-out code should remain.
- No redundant code — factor out repeated logic into helper functions.
- Minimal public interface: Prefer `private` fields and methods when possible.
- Use standard libraries over custom implementations for common operations (e.g., `std::string` operations).


# Folder Structure

- abstracttcpclient/   # Abstract TCP client implementation and interfaces
- abstracttcpserver/   # Abstract TCP server implementation and interfaces
- bite/                # Custom data field and manager implementations
- blockfinalize/       # Logic for block finalization in consensus
- blockproposal/       # Logic for block proposal in consensus
- build/               # Build artifacts and temporary build files
- catchup/             # Catch-up logic for out-of-sync nodes
- chains/              # Chain management and configuration
- cmake/               # CMake build configuration scripts and modules
- cppzmq/              # C++ bindings for ZeroMQ
- crypto/              # Cryptographic primitives and helpers
- datastructures/      # Core data structures used throughout the project
- db/                  # Database access and storage logic
- deps/                # External dependencies
- docs/                # Documentation and guides
- exceptions/          # Custom exception classes and error handling
- flatb/               # FlatBuffers serialization logic
- headers/             # Project-wide header files
- json/                # JSON parsing and utilities
- jsoncpp/             # JSON for Modern C++ (external or wrappers)
- libBLS/              # BLS signature library and threshold encryption
- libjson-rpc-cpp/     # JSON-RPC C++ library
- libzmq/              # ZeroMQ library
- messages/            # Message definitions and serialization
- monitoring/          # Monitoring and metrics collection
- network/             # Networking logic and protocols
- node/                # Node logic, consensus interfaces, networking
- oracle/              # Oracle logic and integrations
- oracle_test/         # Oracle-related tests
- pendingqueue/        # Pending transaction or message queues
- pricing/             # Pricing logic and fee calculations
- protocols/           # Protocol definitions and implementations
- rlp/                 # RLP encoding/decoding logic and utilities
- run_sgx_test/        # SGX test runners and helpers
- scripts/             # Helper scripts for build, test, deployment
- sgxclient/           # SGX client logic and integrations
- sgxwallet/           # SGX wallet logic and integrations
- spdlog/              # Logging library (spdlog)
- statusserver/        # Status server and health endpoints
- test/                # General tests
- thirdparty/          # Third-party libraries and code
- threads/             # Threading and concurrency utilities
- unittests/           # Unit tests
- utils/               # Utility functions and helpers