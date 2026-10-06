# I4-004 Acceptance Criteria

- [x] All requirements traced to I4-004 are implemented and evidenced.
- [x] I3 contracts remain unchanged.
- [x] Debug and Release builds pass.
- [x] Unit/integration/sanity tests pass.
- [x] Full KOS-I2 regression passes.
- [x] Full KOS-I3 regression passes.
- [x] ASan passes.
- [x] UBSan passes.
- [x] Applicable mutation/fault-injection checks pass.

- [x] Review checklist/evidence is complete.
- [x] One atomic commit: the commit whose subject is `KOS-I4 I4-004 implement edge host and remote service` (see `git log`; a SHA cannot be recorded inside its own commit).
- [x] Core R1.0 source is unchanged.
- [ ] HUMAN SAFETY REVIEW — OPEN. This is an independent human-owned gate. It is not completed by software implementation or automated verification. It blocks formal KOS-I4 closure and physical actuator deployment, but does not block subsequent software tasks.

## Scope (architect ruling)

Implemented here: `EdgeHost` over a plain I3 `DeviceRegistry` (no Edge `DeviceManager` or `RuntimeHost`); frame intake; HELLO and sessions (only an accepted HELLO changes session state; a rejected one has no observable effect; an accepted one replaces the session as an Edge-side safety action; HELLO is not idempotent and has no deduplication); the per-session request/notice watermark; discovery with the typed capability mapping; address validation; configure, lifecycle, read and observe on the existing I3 endpoints with their own errors returned unchanged; the actuator write path in the order of protocol section 12, including the session-scoped write ledger (SR-001/SR-002 mechanism); reception of heartbeats. Deferred to I4-006: heartbeat transmission, timeout supervision, stop-all-actuators on timeout, FAULT_EVENT, DEGRADED and the end-to-end SR/FRL campaign. Deferred to I4-005: everything Nexus-side (correlation, outstanding requests, deadlines).

## Evidence (I4-004)

Deliverable: `hardware/remote/` `edge_host.hpp/.cpp`, `capability_dispatch.hpp/.cpp` (the remote library is now a static library); tests `edge_session_test`, `edge_service_test`, `edge_write_test`, `capability_dispatch_test`, `edge_transport_test`, `remote_hygiene_test`, with the rig `tests/hardware/support/edge_rig.hpp` that plays the Nexus with hand-built frames; protocol sections 9 and 12 clarified; SR-001/SR-002 traceability updated.

Architect constraints (I4-003 final review and the I4-004 scope ruling):

| Constraint | Evidence |
|---|---|
| EdgeHost is not a Core Component; no Edge RuntimeHost, RuntimeManager or DeviceManager | `remote_hygiene_test` fails on any such token or include; the remote library links only the hardware abstraction and the transport |
| I3 contracts unchanged; no I4 method added to Device or Endpoint | `git diff` of `hardware/abstraction`, `hardware/mock` and `core` is empty |
| Actuator write only through the existing I3 endpoint; no bypass | the write reaches hardware only through `ActuatorEndpoint<MotorCommand>::write` (capability dispatch); tests show the endpoint's own state check, limit validation, counters and last error; a stopped, faulted or limit-violating write changes nothing |
| Concrete typed endpoint coupling isolated behind capability dispatch | `remote_hygiene_test` forbids `dynamic_cast`, the typed endpoint and sample types and any mock in `edge_host.*` |
| Typed capability mapping, no silent generic endpoint | `capability_dispatch_test`: exactly one capability, one of the four ids, matching direction and the real typed endpoint; lookalikes, unknown ids, two capabilities and no capability are UNSUPPORTED; discovery then serves no topology |
| I4-001 admission rules above the transport; the response-after-heartbeat rule preserved | watermark per session for requests and notices only; a response-like frame neither raises nor is compared with it (`test_a_response_like_frame_does_not_touch_the_watermark`) |
| A rejected HELLO has no observable effect on the current session | `test_rejected_hello_has_no_effect_on_the_session`: 12 rejected variants against a live session with a running actuator and a ledger; session id, actuator and sensor state, velocity, counters, ledger resend and the watermark are unchanged |
| Accepted HELLO replaces the session as a safety action | `test_hello_replaces_the_session_as_a_safety_action` (running actuator stopped, sensors untouched, old-session frames dropped, no ledger inherited), `test_replacement_stops_only_running_actuators`, `test_a_failing_stop_is_counted_and_does_not_block_the_others`, `test_first_hello_stops_nothing` |
| Duplicate HELLO is a protocol semantic, not deduplicated | `test_duplicated_hello_replaces_the_session` and the link-duplicate case in `edge_transport_test` |
| Disconnect is not safety behavior | `test_link_loss_is_connectivity_only_for_the_edge`: ten virtual seconds after a disconnect the actuator is still running and the session intact |
| Duplicate, stale, reordered and lost-response writes | `edge_write_test` (each step of section 12, per-endpoint ledger, non-finite commands never reach the endpoint) and `edge_transport_test` (a link-duplicated write applied once, a reordered older write never applied late, a lost response then an explicit retransmission answered from the ledger without a second apply) |

- Build: clean Debug and Release, 0 warnings.
- Regression: `ctest` 133/133 in Debug and Release (76 Core + 18 KOS-I2 + 21 KOS-I3 + 18 I4); `make check` PASS; `git diff --check` clean.
- ASan and UBSan: all new tests and the transport tests clean.
- Mutation: 70 mutations of `edge_host.cpp` and `capability_dispatch.cpp` (session, HELLO, intake, watermark, malformed handling, discovery, address, configure, lifecycle, status mapping, text, observe, read, the write path and the dispatch). 56 were caught by the first test set; 8 survivors exposed real test gaps (the HELLO sequence not replayable as a request, only running actuators stopped, a failing stop counted, a hardware TIMEOUT never sent as TIMEOUT, observe counters, the ledger keyed by device and endpoint, the cached response content) and are now caught by new tests. 6 equivalent mutants remain: the explicit reset of the watermark and of the old session id (both overwritten a few lines later), the duplicate-path node check and the capability check at step 2 and the sensor-kind check in `read_sensor` (each repeated by the dispatch with the same message), and the ledger stale check at step 4 (never below the session watermark). They are kept as defense in depth and as the literal steps of the specification.
- Core R1.0, the I3 contracts, the mocks and the I4-001..I4-003 code are unchanged.

**Known limitations.** HELLO is not idempotent (above). A rejected write is not recorded, so a retransmission of a rejected write is stale and unanswered (the Nexus sees a timeout). The Edge replies carry text cut to the wire rules (printable ASCII). Heartbeat supervision and FAULT_EVENT are not implemented (I4-006).
