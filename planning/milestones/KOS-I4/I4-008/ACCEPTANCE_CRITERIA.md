# I4-008 Acceptance Criteria

- [x] All requirements traced to I4-008 are implemented and evidenced.
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
- [x] One atomic commit: the commit whose subject is `KOS-I4 I4-008 implement nexus edge reference demo` (see `git log`; a SHA cannot be recorded inside its own commit).
- [x] Core R1.0 source is unchanged.

## Scope (architect ruling)

The reference demo `kritva_nexus_edge_demo` (`examples/nexus_edge_demo/`), software only (Linux host, simulation, no hardware, no wall clock, no threads, no sleeping), exercising the application-level path, not the lower-level protocol again. It uses only: Application, RuntimeHost, DeviceManager, RemoteDevice, RemoteEndpoint (through the unchanged I3 interfaces), RemoteNode, LoopbackLink, SimulatedTransport, EdgeHost, I3 Endpoint, mock hardware. No new runtime manager, device manager, lifecycle framework or safety manager; no change to Core R1.0, the I3 contracts, the runtime, the mocks or the transport; no `observe_edge()`; diagnostics passive; no automatic recovery; the Edge remains the actuator-safety authority.

## Evidence (I4-008)

Deliverable: `examples/nexus_edge_demo/` (`nexus_edge_demo_app.hpp/.cpp`, `main.cpp`, `nexus_edge_demo.conf`, `nexus_edge_demo_clean.conf`, `README.md`, `CMakeLists.txt`); tests `nexus_edge_demo_test` (system, in process), `run_nexus_edge_demo_sanity.cmake` (the executable) and `nexus_edge_demo_hygiene_test` (source scan); a one-line additive alias `RemoteNodeConfig` in `remote_node.hpp` so that the application configures a node without naming the session layer; documentation (demo README, remote architecture, DEMO_PLAN, the DR clarification asked for in the I4-007 review).

Architect constraints and where they are met:

| Constraint | Evidence |
|---|---|
| Use only the production path; never `RemoteSession` from the demo | `nexus_edge_demo_hygiene_test` requires each layer to be used (RuntimeHost, DeviceManager, RemoteNode, LoopbackLink, EdgeHost, SimulatedTransport, `find_endpoint`, `host.initialize/start/stop/shutdown/configure/observe`, `remote::diagnose`, `loop.advance/attach/peer_tick`, the I3 `dynamic_cast` types) and forbids the session layer (`RemoteSession`, `remote_session.hpp`, `session()`), every concrete remote endpoint class, `RemoteProxy`, retransmit, `enter_fault`, `observe_edge`, `service(`, `poll(`, `reopen(`, `open(`, `close(`, `send(`, `receive(`, any clock, thread, sleep or socket, and a `RemoteDeviceManager`; checked by trying 12 bypasses (the session type, `node.service()`, `link.advance`, a direct write on the Edge motor, `inject_fault`, `<chrono>`, `edge.poll()`, `node.session().close()`, a concrete remote endpoint, a second link loss, `sleep_for`, `reopen`): all rejected |
| `LoopbackLink` is the only explicit same-process driver | the scan requires every `.advance(` to be `loop.advance(`, forbids `link.advance(` and `link.connect(`, and allows exactly one `link.disconnect()`: the failure injection |
| No hidden control of the Edge | the Edge's hardware is only observed: only `effective_velocity()` is read from the simulated plant, and no direct call reaches an Edge endpoint |
| No new manager, no Core or I3 change, no observe_edge, passive diagnostics, no automatic recovery | `git diff` of `core/`, `runtime/`, `hardware/abstraction`, `hardware/mock`, `hardware/transport` is empty; the scenario step 7 shows two virtual seconds, observation and diagnostics changing and sending nothing; recovery only by `stop, shutdown, initialize, start` |
| Demonstrates discovery, runtime configuration, initialize and start, sensor read, actuator write, runtime observation and diagnostics, heartbeats, simulated link loss, the Edge's safe stop, Nexus fault visibility, explicit recovery, a second live period | the transcript (nine steps, 38 checked expectations): HELLO and discovery of 2 devices and 4 endpoints; the configuration validated against the allow-list; READY then RUNNING with a fresh session; first sensor samples; a command applied by the Edge and an out-of-limit one refused by the Edge's own validation; the I2 observation and the I3 diagnostics; 5 heartbeats from each side in 500 ms with no degradation; hostile traffic (a duplicated write applied once and answered from the ledger, the actuator endpoint's own counter at exactly one write, a duplicated answer rejected at once and a late one rejected, a replayed older write dropped as stale and never applied, the counter at exactly three writes); link loss: 10 ms later the Nexus has 4 endpoints FAULT (`link lost`, origin LINK_LOST), 4 ERROR events and one link record while the Edge's actuator still runs, then at its own timeout the Edge stops the actuator (safe zero) and keeps its sensors; the runtime health UNHEALTHY; 2 s later nothing changed; recovery to a fresh session 3 with every endpoint RUNNING and the loss history kept; a command applied; heartbeats flowing; controlled shutdown, everything STOPPED, 4 error events in total; `RESULT: PASS`, exit 0 |
| Software-only, simulation-only | no hardware, wall clock or thread; the output is stamped in virtual time and is byte-identical between runs (a test and the sanity script compare two runs) |
| One atomic commit; human safety review visibly retained | this record; the item below is unchecked |

- Build: clean Debug and Release, 0 warnings.
- Regression: `ctest` 145/145 in Debug and Release (76 Core + 18 KOS-I2 + 21 KOS-I3 + 30 I4); `make check` PASS; `git diff --check` clean.
- ASan and UBSan: the demo, its tests and all Edge, Nexus, diagnostics and loopback tests clean.
- Configuration: an invalid or misspelled value (a missing `runtime.name`, a link timeout below two periods, a zero request timeout, `demo.run_ms=5`, a latency above 50 ms, a non-boolean, a non-numeric timing, a misspelled `shoulder_motor.command.max_rad` or `link.heartbeat_period`) is rejected before anything runs (no HELLO, no READY); a different link timing changes the transcript reproducibly (heartbeats over 500 ms: 10 per side at a 50 ms period).
- End-to-end mutation (the demo as a regression guard of the whole stack): 20 deliberate breaks of the production code the demo narrates (the Edge not stopping its actuator on timeout, stopping sensors too, never timing out, reapplying a duplicate write, applying a stale write, the Nexus not faulting live endpoints, losing the fault origin or the link record, sending no heartbeat, not noticing a down link, accepting a duplicate answer or ignoring an unknown one, the loopback never servicing either side, a proxy skipping the fresh session or the recovery cleanup). The first set of demo checks missed 3 (a duplicate write reapplied, a late duplicate answer, and each of the two stale-write protections alone, which are redundant with each other); the demo now also checks the Edge actuator endpoint's own write counter and a late duplicate answer; all 20 are caught (the two stale-write protections jointly).
- Core R1.0, the I3 contracts, the mocks, the runtime and the transport are unchanged.

**Known limitations.** Simulation only; nothing here is evidence for a physical actuator. The demo's Edge-side verification reads the simulated plant (`effective_velocity()`) and the Edge's passive diagnostics, which a real Nexus could not do; that is deliberate for the same-process simulation. The CI workflow runs Debug and the full test suite but not Release or the sanitizers (a separate task after this one). Active observation of the Edge over the link (`observe_edge`) is deferred.
