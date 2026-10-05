# I4-001 Acceptance Criteria

- [x] All requirements traced to I4-001 are implemented and evidenced.
- [x] I3 contracts remain unchanged.
- [x] Debug and Release builds pass.
- [x] Unit/integration/sanity tests pass.
- [x] Full KOS-I2 regression passes.
- [x] Full KOS-I3 regression passes.
- [x] ASan passes.
- [x] UBSan passes.
- [x] Applicable mutation/fault-injection checks pass.

- [x] Review checklist/evidence is complete.
- [ ] One atomic commit SHA is recorded.
- [x] Core R1.0 source is unchanged.

## Evidence (I4-001)

Deliverable: the protocol contract `docs/architecture/KOS-I4_PROTOCOL.md` (frame layout with byte offsets; all 23 message types with numeric ids, directions and response pairing; payload layouts; status mapping onto the released Core `ErrorCode`; version negotiation; session state table; sequence, correlation, duplicate, stale, deadline and heartbeat rules; Edge-authoritative write ledger; discovery, typed mapping and topology equivalence; link timing keys; malformed and unknown input handling), the contract headers `hardware/transport/include/kritva/hardware/transport/protocol.hpp` (+ `protocol.cpp`) and `hardware/remote/include/kritva/hardware/remote/node.hpp` (`NodeId`, `SessionId`, `EndpointAddress`), and the tests below. No codec, transport or proxy exists yet (I4-002 onward).

- Build: clean `rm -rf build`, Debug and Release: 0 warnings from KOS-I2/I3/I4 code (Core's own `component_context_test` has one GCC 13 warning; Core unchanged).
- Unit: `kritva_protocol_contract_unit` (constants, wire magic bytes, message table, request/response pairing, version negotiation, status mapping bijection, exhaustive 5x5 session transition table, link timing ranges and cross-constraints, key names, limits), `kritva_node_identity_unit` (strong ids, address validity, ids colliding across nodes, deterministic ordering) PASS.
- Spec consistency: `kritva_protocol_spec_consistency` parses the specification tables and compares constants, the header layout, all 23 message types (value, name, direction, response), the status mapping, the 9 session transitions and the timing defaults to the code, in both directions.
- Sanity: `kritva_i4_header_sanity` (each public header compiles alone); configure-time checks that `kritva_hardware_transport` links only `kritva_core` and `kritva_hardware_remote` only `kritva::hardware` and `kritva::hardware_transport`.
- Integration: not applicable to a header-and-specification task (no two components interact yet); stated here per the gate rule.
- Regression: `ctest` 119/119 PASS in Debug and Release (76 Core + 18 KOS-I2 + 21 KOS-I3 hardware/demo + 4 I4-001), complete KOS-I2 and KOS-I3 suites unchanged. `make check` PASS; `git diff --check` clean.
- ASan and UBSan: the three unit tests run clean under `-fsanitize=address,undefined` and under each sanitizer alone.
- Mutation: 15 mutations (9 of the contract code, 6 of the specification document) each fail at least one test; a mutation of `kMaxPayloadSize` is caught by a `static_assert` that breaks the build.
- I3 contracts unchanged; Core R1.0 source unchanged (empty diff of `core/`, `runtime/`, `hardware/abstraction/`, `hardware/mock/`).
- Deviation recorded: `MAX_PAYLOAD_SIZE` is 65492 (`MAX_FRAME_SIZE - HEADER_SIZE`), not the baseline's 65520, which assumed a 16-byte header; the agreed header fields need 44 bytes.
- Architect review of the protocol document is the entry gate for I4-002.

## Architect review (provisional, before the commit was visible on GitHub)

All 11 rulings accepted (MAX_PAYLOAD_SIZE 65492; strict header; 23 message types; Core ErrorCode wire status; Edge-allocated sessions; session states; sequence semantics; Edge write order; malformed handling; discovery rules; frame bound before item limits). The review required these clarifications, applied in the follow-up commit `KOS-I4 I4-001 apply architect review clarifications`:

- A HELLO's invalidation of the previous session is an Edge-side safety action in a stated order (invalidate, stop the old session's actuators through `Endpoint::stop()`, reset sequence and ledgers, establish the new session) and the new session inherits no sequence or ledger state.
- DEGRADED means exactly 2 heartbeat periods without a valid peer frame, returns to CONNECTED on a valid frame, never stops an actuator by itself (safety actions only at the heartbeat timeout), and DISCONNECTED never returns to CONNECTING automatically.
- The write guarantee is worded as at-most-once application at the Edge, outcome UNKNOWN to the Nexus application after a timeout, not end-to-end exactly-once; the highest accepted sequence is kept per peer direction and per session.
- Discovery capacity is documented and enforced: under protocol 1.0 every endpoint has exactly one capability, so the largest legal discovery payload is 43524 bytes (`kMaxDiscoveryPayload`, derived in spec section 13), below `MAX_PAYLOAD_SIZE`; the wire maximum of 16 capabilities per endpoint could not fit and is therefore governed by the rule that the frame bound is the hard limit and an encoder checks the required size first. `MAX_CAPABILITIES_PER_ENDPOINT` is not reduced.

Evidence for the follow-up: 119/119 ctest, `make check` PASS, ASan+UBSan clean on the contract and spec-consistency tests, and 5 further mutations (1 code, 4 specification wording) all caught.

## Architect review of the landed commits (5e9acfc, 6b73c6c): conditional approval, one change required

Findings accepted: items 1 to 6 and 8 to 11 PASS. Item 7 required one change: frame sequence numbers are shared by responses and by heartbeats/notices, and the simulated transport may reorder, so a valid response with a lower sequence arriving after a later heartbeat must not be dropped as stale. Fixed in the follow-up commit `KOS-I4 I4-001 admit responses by correlation`:

- Spec section 10 now has two admission rules: requests and notices (every request type, HEARTBEAT, FAULT_EVENT, uncorrelated PROTOCOL_ERROR) use the per-direction per-session monotonic stale/duplicate rule; responses (every response type, HELLO_ACK, correlated PROTOCOL_ERROR) are admitted by session, correlation id, expected type, address and outstanding-request state and never rejected for a lower sequence. The reordered WRITE_RESPONSE (10) / HEARTBEAT (11) example is in the spec.
- Spec section 16: future relaxations (ignorable flags, trailing bytes) apply only once a negotiated minor version defines them; in 1.0 none exists.
- Contract: `is_response_like()` and `passes_stale_check()` in `protocol.hpp`, with a test of the classification of all 22 non-PROTOCOL_ERROR types and of the reordering scenario; the spec-consistency test checks the new wording.
- Evidence: clean Debug and Release 119/119 (I2 and I3 regression unchanged), `make check`, ASan and UBSan clean on the three unit tests, 5 further mutations (3 code, 2 spec) all caught.

Final approval of I4-001: requested from the planning review on the landed follow-up commit; I4-002 starts when it is granted.
