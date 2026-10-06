# I4-005 Acceptance Criteria

- [x] All requirements traced to I4-005 are implemented and evidenced.
- [x] I3 contracts remain unchanged.
- [x] Debug and Release builds pass.
- [x] Unit/integration/sanity tests pass.
- [x] Full KOS-I2 regression passes.
- [x] Full KOS-I3 regression passes.
- [x] ASan passes.
- [x] UBSan passes.
- [x] Applicable mutation/fault-injection checks pass.
- [ ] HUMAN SAFETY REVIEW — OPEN. This is an independent human-owned gate. It is not completed by software implementation or automated verification. It blocks formal KOS-I4 closure and physical actuator deployment, but does not block subsequent software tasks.
- [x] Review checklist/evidence is complete.
- [x] One atomic commit: the commit whose subject is `KOS-I4 I4-005 implement remote device and remote endpoint` (see `git log`; a SHA cannot be recorded inside its own commit).
- [x] Core R1.0 source is unchanged.

## Scope

Implemented here (Nexus side): `RemoteSession` (HELLO, discovery, synchronous request/response with correlation, an outstanding-request table, deadlines on the virtual clock, admission of responses by correlation and never by the sequence watermark, notices with a per-session watermark, the explicit same-sequence retransmission of a write), `RemoteNode` and `RemoteDevice` (devices and typed endpoints built from the discovery; a live period that ties initialize to a fresh session with a set-based topology comparison), the four `RemoteEndpoint` proxies on the unchanged I3 typed contracts, `topology.hpp`. Deferred to I4-006: heartbeat transmission and timeout, link-loss policy (remote endpoints to FAULT, one ERROR each), FAULT_EVENT handling, DEGRADED, the end-to-end safety campaign. Deferred to I4-007: the formal RuntimeHost integration and diagnostics (a DeviceManager test here shows the principle).

## Evidence (I4-005)

Deliverable: `hardware/remote/` `remote_session`, `remote_node` (with `RemoteDevice`), `remote_endpoint`, `topology`; tests `topology_test`, `remote_session_test`, `remote_endpoint_test`, `nexus_edge_test`, the extended `remote_hygiene_test`, and the rig `tests/hardware/support/nexus_rig.hpp`; architecture and RR requirement clarifications.

Architect constraints (I4-004 review) and where they are met:

| Constraint | Evidence |
|---|---|
| Safety authority stays at the Edge | `test_writes_are_validated_by_the_edge_not_the_nexus`: an out-of-limit command IS sent and refused by the Edge with its own error and message (equal to what the Edge endpoint says when written directly); only encodability is checked locally (a non-finite value is never sent); the Edge's state check answers for a stopped endpoint |
| RemoteEndpoint preserves the unchanged I3 Endpoint contract; no I4 API leaks into Core or I3 | the proxies derive from `SensorEndpoint<T>` and `ActuatorEndpoint<MotorCommand>`; the tests use only I3 calls; a local read before RUNNING is refused by the I3 check without sending; I3 counters and last error are the endpoint's own; `git diff` of `core/`, `hardware/abstraction`, `hardware/mock`, `hardware/transport` is empty |
| Synchronous call uses the deterministic pump; no thread, sleep or wall clock | `RemoteSession::exchange` pumps in `pump_quantum` steps and never past the deadline (tested with a quantum of 7 ms against a 100 ms deadline); the hygiene test forbids clocks, threads, sleeping and sockets in every Nexus source; the peer is driven only through the injected peer tick |
| Correlation, expected type, outstanding table, deadline, late, unknown, duplicate rejection, reordering; no global watermark on responses | `remote_session_test`: late (counted separately from unknown), duplicate at a later and at the same instant, unknown correlation, wrong session, wrong type, a response after a later heartbeat accepted, a malformed matching response and a matching PROTOCOL_ERROR failing the request at once (virtual time not advanced), a deadline due exactly at the bound accepted and one nanosecond later timing out |
| TIMEOUT is local, never on the wire; a timed-out write is UNKNOWN; explicit retransmission with the same sequence gets the cached result without a second application | `test_a_timed_out_write_has_an_unknown_outcome_and_retransmission_is_explicit` and the session test: the Edge applied once, answered from its ledger once, the retransmission reuses the sequence, nothing is retransmitted automatically; refused for a request of another session or without a request |
| initialize performs topology verification, set-based | `test_initialize_compares_the_topology_as_a_set` (another registration order: equivalent; a different topology: CONFIGURATION_ERROR, the session DISCONNECTED, the endpoint FAULT, then shutdown, initialize, start works) and `topology_test` (every element of the identity tuple matters, order does not) |
| No RemoteDeviceManager; DeviceManager unchanged | the remote devices register in the existing `DeviceManager`: `test_remote_devices_run_inside_the_existing_device_manager` runs configure, initialize (one fresh session for the whole period), start, read, write, stop, shutdown and a second period; the hygiene test forbids `DeviceManager` and a `RemoteDeviceManager` in the Nexus sources |
| Safety gate remains OPEN | the HUMAN SAFETY REVIEW item below stays unchecked |

Property check (`nexus_edge_test`): 40 seeded campaigns of 60 writes each over a link that drops 20 %, duplicates 25 % and reorders 25 % of the frames in both directions, with explicit retransmissions after some timeouts. Invariants asserted: no value is ever applied twice, the Edge's I3 write counter equals the ledger's applications, there are never more applications than writes, and applied values appear in the order written (a stale command is never applied after a newer one). The campaign provably exercised the faults (over 100 timeouts, over 50 stale drops, resent answers).

- Build: clean Debug and Release, 0 warnings.
- Regression: `ctest` 137/137 in Debug and Release (76 Core + 18 KOS-I2 + 21 KOS-I3 + 22 I4); `make check` PASS; `git diff --check` clean.
- ASan and UBSan: all Nexus and Edge tests clean.
- Mutation: 63 mutants of `remote_session`, `remote_endpoint`, `remote_node` and `topology` (handshake, state, exchange and deadline, admission, notices, write and retransmission, the proxies, the node and the topology). 51 caught by the first test set; 11 survivors exposed real test gaps (the HELLO_ACK fields checked against the HELLO, the typed mapping checked by the Nexus itself, the states during a reopen, the pump running past a deadline that is not a multiple of the quantum, a retransmission changing late into duplicate, the liveness time refreshed by a response, the pending cleanup done once, the position value and timestamp) and are now caught by new tests; 1 equivalent mutant remains (a second endpoint joining a live period without a session fails with the same error from the request either way).
- Core R1.0, the I3 contracts, the mocks and the I4-001..004 code are unchanged.

**Design decisions worth a human reader's attention.** (1) A shutdown with nobody reachable succeeds locally and marks a pending remote cleanup; the next initialize first brings the Edge endpoint to STOPPED (a best-effort STOP, a SHUTDOWN) because sensor endpoints keep running on the Edge when a session ends and the I3 initialize needs STOPPED. This makes the explicit recovery path (shutdown, initialize, start) work after a link loss. (2) The integrator declares the Edge's setting names per endpoint (protocol 1.0 does not carry them). (3) A topology mismatch at initialize is CONFIGURATION_ERROR. (4) Floating-point settings are refused locally (protocol 1.0 cannot carry them).

**Known limitations.** No link-loss policy: after a link loss a proxy stays in its state and every request returns RESOURCE_UNAVAILABLE until an explicit recovery (I4-006). Notices are admitted and counted but their content is not acted on. Diagnostics of the remote side (`observe`) exist on the session but are not exposed through the I3 endpoint (I4-007).
