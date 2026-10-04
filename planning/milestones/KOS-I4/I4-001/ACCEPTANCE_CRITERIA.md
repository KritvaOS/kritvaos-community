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
