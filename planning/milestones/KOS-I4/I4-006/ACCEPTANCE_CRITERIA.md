# I4-006 Acceptance Criteria

- [x] All requirements traced to I4-006 are implemented and evidenced.
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
- [x] One atomic commit: the commit whose subject is `KOS-I4 I4-006 implement distributed failure handling and actuator safety` (see `git log`; a SHA cannot be recorded inside its own commit).
- [x] Core R1.0 source is unchanged.

## Carry-forward from the I4-004 architect review

- [x] The heartbeat campaign decides and tests exactly which frames refresh the Edge liveness timestamp (protocol section 11: any valid in-session frame): a normal admitted frame, an admitted duplicate WRITE (answered from the ledger by `EdgeHost::handle_write` without calling `admit()`, so it does not refresh `last_valid_frame_ns` today), a stale WRITE, a malformed frame, a wrong-session frame and a heartbeat. I4-004 was not reopened for this. Decided in I4-006 by architect ruling: a duplicate WRITE answered from the ledger does NOT refresh liveness (the existing path is correct and is now documented and tested); see the evidence below.

## Architectural statement (architect ruling)

**The Edge is the final actuator-safety authority. Nexus supervision protects the correctness of the remote representation; Edge supervision protects the physical actuator.** Safety invariant: no amount of duplicate or replayed WRITE traffic may prevent an otherwise healthy Edge session from reaching the heartbeat timeout and stopping its RUNNING actuators once fresh valid traffic has ceased.

## Scope

Implemented here: heartbeat transmission on both sides (one per period, never a burst); supervision against the virtual clock only when driven (`EdgeHost::poll()`, `RemoteNode::service()`, every step of the synchronous pump); the `>=` heartbeat timeout; the Edge's one-transition session end (id permanently invalid, watermark and ledgers cleared, every RUNNING actuator stopped through the existing `Endpoint::stop()`, sensors untouched, no Edge endpoint faulted); DEGRADED as observation on both sides; the liveness table; the Nexus link-loss policy (every live remote endpoint FAULT with a deterministic reason through `Endpoint::enter_fault`, one ERROR each through the existing DeviceManager fault listener, one link record); explicit close; FAULT_EVENT (an Edge fault told after the response, applied on the Nexus with `remote fault: <reason>`); no automatic recovery. Not here: formal runtime integration and diagnostics exposure (I4-007), the demo (I4-008).

## Evidence (I4-006)

Deliverable: supervision, `link_state()`, heartbeat sending, fault listeners and FAULT_EVENT in `EdgeHost`; `RemoteSession::service()`, `end_session`, notice handling, link-lost and fault-event handlers; `RemoteNode` link-loss and fault-event policy, `LinkLossRecord`; the proxies' `bind`/`fault` hooks; protocol section 11 rewritten; remote architecture policy section; SR and FRL rules. Tests: `edge_supervision_test`, `remote_supervision_test`, and the updated `edge_transport_test`, `nexus_edge_test`, `remote_endpoint_test`, `remote_session_test`, with `EdgeRig::call` setting notices aside.

Architect rulings and where they are met:

| Ruling | Evidence |
|---|---|
| A duplicate WRITE must NOT refresh Edge liveness; the safety property is tested directly | `test_replayed_writes_cannot_keep_an_actuator_alive`: a write applied once, then the same frame replayed every 50 ms for a second with no fresh traffic: duplicates were answered (`writes_resent` > 0), the session ended at the timeout, the actuator is STOPPED with effective velocity 0, and the write was applied exactly once; the same for a stuck stale heartbeat |
| Liveness table (new admitted request or notice, admitted HEARTBEAT, valid request refused by the endpoint: yes; duplicate WRITE, stale, malformed, wrong-session, no-session, unknown or wrong-side frame: no) | `test_which_frames_refresh_liveness` (14 frame classes, checking `last_valid_frame_ns` and survival at 400 ms) and `test_which_frames_refresh_the_nexus_liveness` (heartbeat, FAULT_EVENT, notice PROTOCOL_ERROR, matching PROTOCOL_ERROR and accepted response refresh; stale, wrong-session, malformed, bad-header, wrong-side, late, unknown-correlation, wrong-type and malformed responses do not) |
| `>=` heartbeat timeout, tested one nanosecond before and exactly at | Edge: `test_the_timeout_is_exactly_at_the_bound_and_stops_only_actuators`; Nexus: `test_the_timeout_is_exactly_at_the_bound_and_faults_every_live_endpoint_once` |
| The supervision clock never advances implicitly | `test_supervision_runs_only_when_driven` (ten virtual seconds without driving: nothing changed until the Edge is polled); the source scan forbids clocks, threads, sleeps and sockets |
| No heartbeat bursts | both sides: a 250 ms jump gives exactly one heartbeat; the first one is one period after the session starts |
| A heartbeat never acts | `test_a_heartbeat_never_acts` |
| Timeout is one atomic transition; the old SessionId never regains validity; new HELLO always new session | `test_the_old_session_is_gone_for_good` (old-session heartbeat and a replay of its last write dropped, new HELLO gets a new id, the old id still invalid), `test_a_new_hello_just_before_the_timeout_takes_the_replacement_path` |
| Sensors remain RUNNING; only actuators stop; no Edge endpoint faulted | asserted in the timeout, transport and end-to-end tests; a source mutation that stops sensors too is caught |
| Recovery never clears a Nexus FAULT; no automatic recovery | `test_nothing_recovers_by_itself_and_the_way_back_is_explicit`: the Edge times out and stops the actuator, the Nexus faults 4 endpoints (4 ERRORs, 1 record), two more virtual seconds change nothing and send nothing, a fresh `reopen()` leaves all 4 FAULT, then shutdown, initialize, start works on a new session |
| Fault cardinality: N faults, N ERRORs, 1 diagnostic; repeated driving adds none | `...faults_every_live_endpoint_once` (4/4/1, then 100 more drives: still 4/4/1), `test_an_already_faulted_endpoint_gets_no_second_event` (1 then 3), `test_only_live_endpoints_are_faulted` (UNKNOWN and STOPPED left alone), through the existing `DeviceManager` event sink |
| Reasons `link lost`, `session closed`, `remote fault: ...`; explicit close faults live proxies and is not an Edge safety event | `test_a_transport_that_goes_down_ends_the_session_at_once`, `test_an_explicit_close_faults_the_live_endpoints_with_its_own_reason` (the Edge keeps its session and actuator until its own timeout), `test_a_fault_event_faults_the_matching_live_proxy_once` |
| FAULT_EVENT: transition only, queued after the response, safe if lost, never an Edge safety action | `test_a_fault_outside_a_request_is_told_at_the_next_poll` (FAULT to FAULT: none; a new transition after the way back: one), `test_a_fault_caused_by_a_request_is_told_after_its_response` (response first), `test_a_failing_lifecycle_request_is_answered_then_the_fault_is_told`, `test_a_fault_with_no_session_is_not_told`, `test_a_fault_while_the_session_is_replaced_is_not_told_to_the_new_one`, `test_an_edge_fault_reaches_the_nexus_end_to_end`, bad events (wrong node, stale, malformed, wrong session) do nothing |
| Response admission by correlation preserved; no global watermark on responses | the I4-005 admission tests still pass unchanged; late, duplicate and unknown responses are rejected and never count as liveness |

The I4-004 test that asserted "link loss is connectivity only" contradicted the I4-006 policy and was rewritten to assert the stop (`test_link_loss_ends_the_edge_session_and_stops_the_actuators`). Heartbeats consume frame sequence numbers; the tests that assumed exact numbers were adapted. The I4-005 fault campaign now drives both sides while idle and treats a legitimately ended session as the end of that run.

- Build: clean Debug and Release, 0 warnings.
- Regression: `ctest` 139/139 in Debug and Release (76 Core + 18 KOS-I2 + 21 KOS-I3 + 24 I4); `make check` PASS; `git diff --check` clean.
- ASan and UBSan: all Edge, Nexus and transport tests clean.
- Mutation: 61 mutants of the supervision, liveness, link-loss and FAULT_EVENT code on both sides. 56 caught by the first test set; 2 survivors exposed real gaps (the Nexus's first heartbeat time, a FAULT_EVENT matching by device only) and are now caught; 3 equivalent mutants remain: the explicit ledger clear at the timeout (a new HELLO clears them again before any frame can use them), the guard in the FAULT_EVENT flush for a session that ended after the event was queued (the timeout already discards the queue), and the DEGRADED-to-CONNECTED step in `refresh_liveness` (`service()` makes the same transition).
- Core R1.0, the I3 contracts, the mocks and the transport are unchanged.

**Known limitations.** Supervision runs only when driven; a gap in driving is a gap in supervision. A frame's liveness time is the time it is handled, so a backlog read after a gap counts from that moment (conservative for the Nexus, irrelevant for the Edge's safety). The Edge learns that the Nexus closed its session only by the heartbeat timeout (protocol 1.0 has no termination message). Remote diagnostics through the I3 endpoint and the exposure of the link record are I4-007.
