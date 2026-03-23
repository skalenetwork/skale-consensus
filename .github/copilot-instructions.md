## PR Review Priorities

Review in this order:
1. Correctness, undefined behavior, and incomplete logic
2. Performance regressions in hot paths
3. Concurrency and thread-safety issues
4. Ownership, lifetime, and resource-management bugs
5. API / architecture violations
6. Missing or weak tests

## Review Behavior

- Focus on changed code and the nearby impact radius, not only the modified lines.
- Prefer high-signal findings over style or formatting nits.
- Do not restate obvious code or give generic praise.
- Prioritize production risk, determinism, safety, and latency.

For each issue:
- Classify as Critical / Major / Minor
- Explain why it matters
- Point to the risky code path or assumption
- Suggest a concrete fix or safer alternative

## C++ Correctness and Safety

Flag especially:
- dangling pointers, references, iterators, views, spans, or captured objects
- use-after-move, moved-from misuse, or invalid lifetime assumptions
- uninitialized state, partial initialization, invalid ownership transfer, or double-free risk
- raw `new`/`delete`, manual resource handling, or missing RAII
- exception-safety problems, partial state updates, or cleanup gaps
- undefined behavior from bounds errors, overflow, aliasing, invalid casts, or invalid memory access
- incorrect assumptions about container invalidation, iterator stability, or object lifetime

## Performance Review

In hot paths, flag:
- hidden allocations, copies, temporary objects, string formatting, or redundant conversions
- unnecessary `shared_ptr` copies, pass-by-value of heavy objects, or missed move opportunities
- algorithmic complexity regressions
- repeated work inside loops
- blocking I/O, expensive syscalls, or serialization overhead
- excessive logging, debug code, or observability overhead on critical paths

## Concurrency Review

Flag:
- data races or unsafe shared mutable state
- widened lock scope, lock contention, or unnecessary synchronization
- blocking I/O, callbacks, logging, or expensive work while holding locks
- deadlock risk due to inconsistent lock ordering
- incorrect atomic usage, memory ordering assumptions, or missed wakeup / predicate issues
- non-deterministic behavior in consensus, shared state, or cross-thread handoff

## API and Design

Flag:
- unclear ownership boundaries or fragile lifetime requirements
- misuse of `std::move`, `std::forward`, `string_view`, `span`, references, or callbacks
- APIs that hide costs, side effects, or lifetime constraints
- tight coupling, cross-layer violations, or unnecessary complexity
- behavior changes that break invariants, compatibility, or error semantics

## Conventions That Matter

Flag:
- missing const-correctness where it affects API clarity, safety, or copies
- ad-hoc logging (`std::cout`, ANSI color output) in production paths
- ignored return values or silently dropped errors
- public behavior changes without tests
- missing assertions, validation, or invariant checks at critical boundaries

## Repo-Specific Priorities

Critical areas:
- `network/`: latency-sensitive; avoid blocking, retries with poor bounds, and extra roundtrips
- `crypto/`: correctness first; require tests or vectors for logic changes
- `db/`: avoid inefficient queries, hidden copies, and lock contention
- `bite/` core BITE encryption / merge / decryption / encoding / decoding logic

Hot paths include:
- request handling
- consensus / block processing
- serialization / deserialization
- message validation and dispatch

## Testing and Observability

- Flag behavior changes without corresponding tests.
- Suggest edge-case, failure-path, load-sensitive, and concurrency tests where relevant.
- Ensure critical paths remain observable with lightweight metrics or structured logs.
