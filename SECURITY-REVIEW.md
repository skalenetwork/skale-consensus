# Consensus external-input security review

Date: 2026-10-15

Reviewed revision: `d8124954e0dfcaaeb39bcc534e5e6f0a0abc801e`

## Scope and methodology

This defensive review covers externally supplied consensus messages, proposal,
catch-up and finalization traffic, BITE HTTP requests, the visible skaled
transaction callback, and relevant listeners in the bundled SGX wallet.

An initial static audit was followed by an independent static verification of
each finding. This document records the second pass's conclusions and supersedes
stronger preliminary claims. Six findings were confirmed and four partially
confirmed. No source fixes, builds, tests, installations, benchmarks, network
probes, or runtime reproductions were performed.

Severity describes the supported impact and prerequisites. Confidence describes
the static conclusion, not demonstrated production exploitability. Listener
reachability, deployed binaries, runtime arguments, firewall rules, resource
limits, and exploit timing were not verified. No remote code execution,
private-key extraction, or guaranteed network-wide halt was established.

Treat this document as defensive remediation material. Coordinate disclosure
through the repository's [security policy](SECURITY.md); a local documentation
commit does not imply approval to publish unresolved findings.

## Findings summary

Finding IDs remain stable across the two passes.

| ID | Severity | Verdict | Supported finding | Confidence |
|---|---|---|---|---|
| 1 | Medium | Confirmed | Empty HTTP body reaches a null buffer dereference | 10/10 |
| 2 | Medium | Confirmed | Unbounded accepted connections and an escaping accept exception can terminate the process | 9/10 |
| 3 | Medium | Confirmed | Proposal hashes persist before sender/signature authentication | 9/10 |
| 4 | Medium | Partially confirmed | Proposal transaction sizes control uncapped allocation; overflow validation is defective | 9/10 |
| 5 | Low | Partially confirmed | Catch-up size overflow bypasses validation; memory corruption is not established | 9/10 |
| 6 | Medium | Confirmed | Peer-declared finalization fragment size controls an uncapped allocation | 9/10 |
| 7 | Medium | Confirmed | HTTP request handlers leak, with body retention on some paths | 10/10 |
| 8 | Medium | Confirmed | Application-retained HTTP request bodies have no cumulative byte cap | 9/10 |
| 9 | High, conditional | Partially confirmed | Separate wallet service has insecure defaults, but supplied deployment examples enable protection | 9/10 |
| 10 | Low | Partially confirmed | Eligible BITE shares can be retrieved without authenticated caller identity | 9/10 |

Medium severity does not imply low remediation priority: findings 1 and 2 have
supported process-termination paths when their reachability prerequisites hold.

## 1. Empty HTTP body dereferences a null buffer

Evidence: [HTTP handler](bite/server/FlatBufferRequestHandler.cpp), lines 47-48.

For recognized routes, the handler moves the body queue and immediately calls
`coalesce()` before checking whether the resulting pointer exists. The inspected
local Folly implementation returns the queue's existing head without allocating
an empty buffer. Proxygen discards zero-length body input, so a bodyless request
can leave the queue head null.

Framework evidence:
[Folly queue](libBLS/deps/folly/folly/io/IOBufQueue.h),
[queue initialization](libBLS/deps/folly/folly/io/IOBufQueue.cpp), and
[Proxygen transaction handling](deps/proxygen/proxygen/lib/http/session/HTTPTransaction.cpp).

**Prerequisites and impact:** A BITE build on a non-sync-only node, a reachable
HTTP listener, and a bodyless request to a recognized route. Invalid routes
return before this dereference. Null-pointer undefined behavior has an ordinary
Linux process-crash outcome; exception handlers do not catch a segmentation
fault. This was statically verified, not reproduced.

**Fix:** Check for a null moved buffer before member access, then validate length
and return an explicit bounded error response.

**Regression coverage:** Bodyless and zero-length requests to both recognized
routes must return errors without process termination.

## 2. Connection admission can lead to an uncaught accept exception

Evidence: [TCP server](abstracttcpserver/AbstractServerAgent.cpp), queue insertion
at line 47, accept failure at line 146, and direct thread launch at line 169;
[queue declaration](abstracttcpserver/AbstractServerAgent.h).

Accepted sockets enter an unbounded queue. Slow clients can outpace the workers
and retain descriptors. An unsuccessful `accept()` throws
`NetworkProtocolException`, but the accept loop catches only `FatalError`.
These are separate exception subclasses. There is no outer recovery wrapper
around the `std::thread` entry, so an escaping exception invokes
`std::terminate`.

Worker exception handling occurs in a different thread and does not protect the
accept loop. Normal [connection destruction](network/ServerConnection.cpp) closes
sockets; the defect is unbounded retention, not a universal lack of cleanup.

**Prerequisites and impact:** TCP access and enough sustained accepted
connections to outpace workers and produce an accept error, such as descriptor
exhaustion. OS limits and ingress controls affect feasibility. Supported impact
includes process termination.

**Fix:** Bound queued and active connections, reject excess admission, and apply
deadlines measured from acceptance. Handle transient errors and descriptor
exhaustion explicitly with bounded backoff. Preserve RAII ownership during socket
setup and ensure exceptions cannot escape the thread entry.

**Regression coverage:** Idle-client saturation, descriptor exhaustion, transient
accept failures, cleanup, and legitimate-peer service during overload.

## 3. Proposal hashes persist before authentication

Evidence: [proposal server](blockproposal/server/BlockProposalServerAgent.cpp),
hash persistence at line 635 and later signature verification at line 438;
[proposal hash database](db/ProposalHashDB.cpp), first-value persistence and
subsequent equality checks.

The server stores an advertised hash during response-header creation, before
reading and authenticating the proposal. A differing later hash for that
block/proposer is rejected as a double proposal. The write reaches LevelDB; no
signature-failure rollback was found.

**Prerequisites and impact:** The caller must reach the proposal port, know
current chain/roster/state information, satisfy next-block, timestamp,
state-root, local-proposal and DA-proof-count checks, and win the relevant race.
Those checks constrain the attack but do not authenticate the caller.
Unauthenticated state poisoning can suppress later honest proposals, including
after an ordinary restart while the stored entry remains. Forged-block
acceptance and guaranteed permanent chain-wide failure are not established.

**Fix:** Authenticate the signed proposal commitment and claimed identity before
persisting admission/equivocation state. Retain full payload verification.

**Regression coverage:** Invalid signatures, incomplete exchanges and disconnected
clients must not reserve a hash or prevent a subsequent honest proposal.
Authenticated conflicting proposals must still be detected.

## 4. Proposal transaction lengths control uncapped allocation

Evidence: [missing-transaction exchange](blockproposal/server/BlockProposalServerAgent.cpp),
lines 113-120; [transaction bounds and iterator construction](datastructures/Transaction.cpp),
lines 148-154; [list deserialization](datastructures/TransactionList.cpp).

The server sums supplied lengths without checked arithmetic and allocates the
total without per-item or aggregate byte limits. The size-array count is not
matched to the requested missing-transaction count. Earlier limits on transaction
count and JSON-header bytes do not bound numeric length values.

The bundled JSON implementation permits arithmetic conversion of negative
integral values to unsigned lengths. Wrapped `start + length` arithmetic can pass
the deserializer's bounds check and produce a reversed iterator range.

**Important qualification:** The inspected GCC 11 libstdc++ vector constructor
checks the converted range distance and throws before copying; callers catch
these exceptions. This does not establish memory corruption, RCE, or inevitable
process termination from the overflow sequence.

**Prerequisites and impact:** Access to the missing-transaction exchange, after
finding 3's state gates and a nonempty missing-hash request. Unbounded
allocation/memory pressure and defective portable bounds validation are
supported. Allocation failure can be caught; OS OOM killing was not demonstrated.

**Fix:** Enforce positive integral lengths, expected array count, per-transaction
and aggregate byte budgets before allocation. Use checked addition and validate
`start <= bufferSize` followed by `length <= bufferSize - start`. Require exact
body consumption.

**Regression coverage:** Invalid numeric values, overflowing totals, inconsistent
counts, oversized allocations, truncation and trailing bytes.

## 5. Catch-up length arithmetic can wrap before validation

Evidence: [catch-up size parsing](catchup/client/CatchupClientAgent.cpp), line 214;
[block-list range construction](datastructures/CommittedBlockList.cpp), lines 71-77.

Peer-controlled lengths are summed before applying the configured cap.
Deserialization also calculates an unchecked endpoint. Unsigned conversion and
overflow can bypass these validation expressions.

**Important qualifications:** A malformed first block ordinarily throws before
later lengths are processed. Reaching a later problematic range requires a valid
first block. On the inspected libstdc++ implementation, the resulting reversed
range is rejected before copying. The block-list handler catches failures and
may return a valid prefix. Block signature verification is not bypassed, and the
wrapped aggregate remains subject to the configured allocation cap.

**Prerequisites and impact:** Control of a selected catch-up peer or its plaintext
response path. The arithmetic validation defect is confirmed; out-of-bounds
copying, memory corruption and deterministic termination are not established.

**Fix:** Validate each length against the remaining download budget before
addition and independently check every slice against actual remaining bytes.

**Regression coverage:** Overflowing totals, invalid numeric conversion, a valid
first block followed by inconsistent metadata, and valid-prefix recovery.

## 6. Finalization fragments allocate before byte-limit enforcement

Evidence: [finalization downloader](blockfinalize/client/BlockFinalizeDownloader.cpp),
size parsing at line 235 and allocation at line 339.

Fragment size is rejected only when zero. It is not constrained by a hard byte
limit or a validated relationship to the declared block size before allocation.
Response status, hash consistency and possible DA-proof processing do not
authenticate the transport's size fields.

DA-proof processing verifies only nonempty signatures at this stage; an empty
signature or an already available proof can avoid verification here. This does
not establish acceptance of an invalid finalized block.

**Prerequisites and impact:** A malicious/compromised selected finalization peer
or control of its response path during an active download. Attacker-directed
allocation and memory pressure are supported. Worker recovery catches allocation
and parsing exceptions, so allocation failure alone does not prove termination.

**Fix:** Enforce a hard fragment cap and protocol-consistent envelope bounds
before allocation, including BITE share and serialization overhead.

**Regression coverage:** Over-limit sizes, invalid sizes paired with otherwise
valid proofs, empty-proof handling, and legitimate maximum-size envelopes.

## 7. HTTP handler cleanup is missing

Evidence: [factory allocation](bite/server/BlockFinalizeHandlerFactory.cpp), line
12; [empty completion callback](bite/server/FlatBufferRequestHandler.h), line 50;
[error callback](bite/server/FlatBufferRequestHandler.cpp), line 133.

The local [Proxygen adaptor](deps/proxygen/proxygen/httpserver/RequestHandlerAdaptor.cpp)
deletes itself, not the application handler. The
[handler callback contract](deps/proxygen/proxygen/httpserver/RequestHandler.h)
requires application cleanup. Neither completion nor error handling releases
the allocated handler.

**Prerequisites and impact:** A reachable BITE HTTP listener. Repeated requests
leak handlers. Invalid-route and pre-completion error paths can additionally
retain accumulated bodies.

**Qualification:** Recognized-route bodies moved into a local owning pointer are
normally destroyed. Not every successful request permanently leaks its full
body.

**Fix:** Implement completion and error cleanup according to Proxygen's ownership
contract, avoiding double deletion.

**Regression coverage:** Handler and buffer counts return to baseline after
successful, rejected, aborted and disconnected requests.

## 8. HTTP application body retention has no cumulative cap

Evidence: [body accumulation](bite/server/FlatBufferRequestHandler.cpp), lines
22-25; [HTTP server options](bite/server/BiteBlockFinalizeServer.cpp).

Every body chunk is appended without a route or cumulative-byte check. Invalid
routes are rejected only at end-of-message. Proxygen's receive windows and stream
limits do not cap bytes already delivered to and retained by this handler.
Coalescing a chained body can allocate a contiguous replacement before releasing
the old chain, increasing peak memory.

**Prerequisites and impact:** A reachable BITE HTTP listener. Memory retention is
attacker-controlled and amplified by finding 7 on invalid/aborted paths. Flow
control and timeouts constrain throughput/lifetime, not total retained bytes.
Process death is not guaranteed merely because allocation can fail.

**Fix:** Enforce per-request and aggregate in-flight byte budgets during body
delivery. Reject invalid routes early and explicitly discard or abort remaining
bodies. Add server/proxy limits as defense in depth.

**Regression coverage:** Oversized valid/invalid routes, concurrent streams,
aborted bodies and measured retained-memory bounds.

## 9. Separate SGX wallet service has insecure authentication defaults

Evidence: [default flags](sgxwallet/ServerInit.h), lines 53-59;
[wildcard ZMQ binding](sgxwallet/zmq_src/ZMQServer.cpp), line 78;
[conditional message authentication](sgxwallet/zmq_src/ZMQMessage.cpp);
[conditional signing ownership checks](sgxwallet/zmq_src/ReqMessage.cpp).

The bundled wallet defaults signature and ownership enforcement to false, starts
ZMQ on all interfaces at port 1031, and permits signing paths without a second
client-identity check when these protections are disabled.

**Disconfirming deployment evidence:** The `-e` option enables both protections.
The supplied [hardware](sgxwallet/run_sgx/docker-compose.yml) and
[simulation](sgxwallet/run_sgx_sim/docker-compose.yml) configurations pass it.
Universal insecure deployment is not established.

**Prerequisites and impact:** A separately running wallet with protections
disabled, network reachability, and knowledge of an existing key identifier.
Unauthorized ECDSA signing or BLS-share signing is supported, not private-key
extraction or compromise of an entire threshold quorum.

**Fix:** Verify production arguments, restrict wallet reachability, and prefer
secure defaults with explicit startup rejection or prominent warnings for
insecure operation.

**Regression coverage:** Unsigned requests and wrong-owner requests fail; intended
authenticated clients continue to work.

## 10. Share retrieval does not authenticate the claimed node identity

Evidence: [finalization request handling](catchup/server/CatchupServerAgent.cpp),
roster lookup at line 213, share request flag near line 347, DA-backed proposal
selection near line 354, and share availability handling near line 405.

The server checks that a supplied node ID belongs to the roster but does not
cryptographically bind that ID to the TCP caller.

**Material release gates:** A proposal with a DA proof or suitable committed
block must be available. A stored DA signature and local shares are required.
The server waits for asynchronous local computation and refuses unavailable
shares. It does not generate shares for arbitrary submitted ciphertext.

**Prerequisites and impact:** Reachability, known chain/roster identifiers, valid
indices, and available eligible shares. One response yields one node's share
list; reconstruction still requires a distinct-node threshold. The ordinary
one-node request path is excluded by fragment-index checks.

**Unresolved:** DA proof is not itself consensus finality, but the intended
confidentiality boundary and a complete threshold-before-decision attack were
not established. Early plaintext disclosure must not be asserted from this
evidence alone.

**Fix:** Authenticate callers and explicitly define/enforce share-release
authorization and phase policy.

**Regression coverage:** Spoofed node identities fail; authenticated retrieval
preserves asynchronous readiness and actual threshold requirements, including
one-node finalization.

## Source-level listener inventory

This is not a live port scan. Port offsets come from
[common definitions](SkaleCommon.h), core configuration from
[Node](node/Node.cpp), and BITE HTTP configuration from
[its server](bite/server/BiteBlockFinalizeServer.cpp).

| Service | Port | Source-level exposure and gates |
|---|---|---|
| Proposal TCP | `basePort + 0` | Configured `bindIP`; no transport authentication found before admission/parsing |
| Catch-up/finalization TCP | `basePort + 1` | Configured `bindIP`; request node ID is not authenticated identity |
| Consensus ZMQ | `basePort + 4` | Configured `bindIP`; examined path limits messages to 4096 bytes and verifies signatures after parsing |
| Status JSON-RPC | `basePort + 10` | Conditional on `CONSENSUS_DEMO`; empty-host connector bind/auth semantics need confirmation |
| BITE HTTP/2 | `basePort + 11` | BITE, non-sync-only nodes; configured `bindIP`; no TLS/application authentication shown in construction |
| SGX wallet HTTPS | 1026 | Separate process; defaults include client-certificate checking; configuration can weaken this |
| SGX registration / CSR / info | 1027 / 1028 / 1030 | Separate process; CSR explicitly localhost; registration/info binding needs confirmation |
| SGX wallet ZMQ | 1031 | Separate process; wildcard binding; authentication depends on runtime options |

## Skaled transaction boundary and residual scope

Consensus obtains transactions through the local
[pending-transactions callback](pendingqueue/PendingTransactionsAgent.cpp), not
a consensus-owned public transaction port.

Returned bytes are copied/deserialized before entering the BITE-specific
catch-and-skip block. No explicit per-transaction or aggregate byte cap was found
at this ingestion point. The [RLP parser](rlp/RLP.h) provides data, item,
list-count and nesting limits, but these do not substitute for an aggregate
memory/work budget.

Public-user reachability of arbitrary callback bytes depends on skaled's
upstream admission rules, which were not established by this repository review.
No additional high-confidence fatal-input finding was established in the
examined bounded consensus ZMQ receive path or RLP field parser. This is not
exhaustive parser certification.

Follow-up should audit skaled's producer and enforce count, per-transaction byte,
aggregate proposal byte and parsing/work budgets at both sides of the boundary.
Malformed-input errors should be distinguished from internal invariant failures.

## Timed defensive plan

### Immediate containment: 0-24 hours

- Restrict peer ports and wallet services to intended clients.
- Restrict unintended BITE HTTP access pending null handling and cleanup fixes.
- Confirm wallet authentication flags and actual exposed interfaces.
- Apply temporary admission/rate controls and monitor descriptors, RSS, restarts
  and unexpected proposal-rejection patterns.

### First code batch: 1-3 days

- Fix findings 1, 2 and 7: HTTP null handling, handler lifecycle, bounded TCP
  admission and accept-error recovery.
- Fix finding 3: authenticate commitments before persistent proposal state.
- Bound HTTP bodies and proposal/finalization allocations.
- Add checked arithmetic and exact numeric/count/slice validation.

### Isolated validation: 3-7 days

- Add the negative tests described above, sanitizer-backed parser coverage and
  controlled memory/descriptor exhaustion tests.
- Require no malformed-input process termination, bounded resources, no
  unauthenticated proposal-state mutation and continued legitimate-peer progress.
- Preserve BITE asynchronous share readiness and actual threshold semantics;
  consensus completion alone is not proof that local shares are ready.

These are proposed validation activities, not commands run during this review.
Builds and tests require explicit approval under repository rules.

### Follow-up: 2-4 weeks

- Audit the skaled transaction path end to end and document shared budgets.
- Fuzz network deserializers and BITE/RLP parsing in isolation.
- Resolve share-release policy and add authorization/phase tests.
- Add deployment checks for unexpected wildcard listeners and disabled
  authentication.

## Corrections to preliminary claims

- Findings 4 and 5 do not demonstrate memory corruption or RCE on the inspected
  standard-library implementation.
- Allocation failure in findings 4 and 6 does not necessarily terminate
  consensus; relevant recovery handlers exist.
- Finding 7 does not mean every completed request leaks its full body.
- Finding 9 does not establish insecure operation of supplied deployment
  examples, which enable protection.
- Finding 10 does not establish premature threshold decryption or bypass of
  threshold cryptography.
- Finding 3 establishes proposal suppression, not forged-block acceptance or
  guaranteed permanent chain-wide failure.
