# I4-003 Acceptance Criteria

- [x] All requirements traced to I4-003 are implemented and evidenced.
- [x] I3 contracts remain unchanged.
- [x] Debug and Release builds pass.
- [x] Unit/integration/sanity tests pass.
- [x] Full KOS-I2 regression passes.
- [x] Full KOS-I3 regression passes.
- [x] ASan passes.
- [x] UBSan passes.
- [x] Applicable mutation/fault-injection checks pass.

- [x] Review checklist/evidence is complete.
- [x] One atomic commit: the commit whose subject is `KOS-I4 I4-003 implement deterministic simulated transport` (see `git log`; a SHA cannot be recorded inside its own commit).
- [x] Core R1.0 source is unchanged.

## Evidence (I4-003)

Deliverable: `transport.hpp`, `simulated_transport.hpp/.cpp`, `link_config.hpp/.cpp` in `hardware/transport/`; tests `simulated_transport_test`, `link_config_test`, `transport_hygiene_test`, `frame_transport_test` (integration with the I4-002 codec).

Architect constraints (I4-002 review) and where they are met:

| Constraint | Evidence |
|---|---|
| Transports encoded frames as opaque bytes; no inspection or modification | `frame_transport_test`: every valid sample message crosses byte for byte and decodes afterwards; garbage, bad magic and a stale sequence cross untouched; the transport has no include of the protocol, frame, codec or node headers (it owns `kMaxTransportFrameSize`, checked equal to the protocol bound by a `static_assert` in the integration test); the hygiene test fails on any such include or on frame, header, session, correlation or protocol symbols, with no exemption (architect review of 15c1781 found the earlier exemption masked an include of `protocol.hpp`); the "bytes modified in flight" mutant is caught |
| Deterministic: no threads, sleep, wall clock, randomness unless seeded | hygiene test scans for `<chrono>`, `<thread>`, `<random>`, sleep and clock calls, sockets; `scripted_run` is repeated with the same seed and gives identical deliveries and times; another seed differs |
| Explicit `step()`/`advance(dt)` only, no hidden background activity | time does not move by `send`, `receive`, `pending`, `stats`; `step()` and `advance()` boundary tests, overflow refused, saturation of due times |
| Latency, drop, reorder, duplicate, disconnect, reconnect | unit tests for each, explicit by send index and probabilistic by seed; the extremes 0 and 1000 permille; queue bound |
| No delivery before the due time; delivery exactly at it | `test_fixed_latency_boundaries` (one ns early, exactly at, afterwards) |
| Disconnect affects connectivity only (no I4-006 policy) | `test_disconnect_is_connectivity_only`: time and counters untouched, nothing generated or queued |
| No real networking | no socket or networking headers; hygiene test |
| Consumes the I4-002 codec; I4-002 immutable | the integration test uses `encode_frame`/`decode_frame`; `git diff` of the I4-002 files is empty |
| No session manager growth | no protocol type appears in the transport; session, correlation and sequence handling is not present |

- Build: clean Debug and Release, 0 warnings.
- Regression: `ctest` 127/127 in Debug and Release (76 Core + 18 KOS-I2 + 21 KOS-I3 + 12 I4); `make check` PASS; `git diff --check` clean.
- ASan and UBSan: the new tests and the I4-002 frame and codec tests clean.
- Mutation: 33 mutations of the transport and the link configuration; 33 caught (one, the clock that does not move in `step()`, by a hang that the ctest timeout reports). Four survived the first set of tests and each exposed a real gap, fixed in the tests: the permille boundary (`<=` instead of `<`), reconfiguring not restarting the send index, the reorder delay having no effect, and a 32-bit configuration value wrapping (2^32 + 50 read as 50). All four are now caught.
- Core R1.0, the I3 contracts and the I4-001/I4-002 files are unchanged.
