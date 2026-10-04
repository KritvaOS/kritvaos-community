# Planning Decisions

- Core R1.0 is treated as a stable dependency for KOS-I2.
- KOS-I2 is Linux-host software only.
- Runtime abstractions remain minimal and evidence-driven.
- Hardware/distributed functionality is deferred to later milestones.

## KOS-I3 implementation alignment (Claude and ChatGPT, 2026-10-04)

Approved before I3-001; these supersede any conflicting wording in the I3 planning text.

| # | Decision |
|---|---|
| 1 | One `DeviceManager` is a single Core `Component` registered with the existing `RuntimeHost::add_component()`. `RuntimeHost`, `RuntimeManager` and Core are unchanged; there is no `add_device()`. Devices and Endpoints are not Components (DER-003). It fans out configure/initialize/start in deterministic order and stop/shutdown in reverse. |
| 2 | Endpoints reuse the released Core `Lifecycle` and the Component operation table (configure/initialize from UNKNOWN or STOPPED, start from READY, stop from READY or RUNNING, FAULT left only by shutdown). No new state enum or state machine (DER-407). |
| 3 | An endpoint fault makes that endpoint FAULT/UNHEALTHY; the `DeviceManager` stays RUNNING but reports Health UNHEALTHY and emits exactly one ERROR event. `stop()` stops healthy endpoints; `shutdown()` releases the faulted one. No automatic recovery. |
| 4 | One typed sample per endpoint (`AccelerationSample`, `AngularVelocitySample`, `PositionSample`, `MotorCommand`) on a small `Vec3` with documented SI units; templates `SensorEndpoint<Sample>` (`read(Sample&)`) and `ActuatorEndpoint<Command>` (`write(const Command&)`). No universal variant. This replaces the single `ImuSample` / `EncoderSample` wording. |
| 5 | `DeviceId` / `EndpointId` are strong types over `core::Id` (non-zero) plus a required unique non-empty name; lookup by id and by name; endpoint ids unique within a Device. |
| 6 | The integrator owns Devices; the registry holds non-owning pointers; a Device owns its Endpoints (valid for the Device lifetime); registration closes at the first `initialize()`. |
| 7 | Mocks live in a reusable library `kritva_hardware_mock` under `hardware/mock/` (depends only on `hardware/abstraction`), used by the tests and by `examples/device_demo`. Not under `tests/`. |
| 8 | Released Core `ErrorCode` only: not RUNNING = `NOT_READY`; FAULT endpoint = `RESOURCE_UNAVAILABLE`; injected hardware failure = `INTERNAL_ERROR`; invalid argument or out-of-range command = `INVALID_ARGUMENT`; invalid lifecycle call = `INVALID_STATE`. `Error.source` is the `DeviceManager` component id; device and endpoint identity are carried in endpoint diagnostics. |
| 9 | Samples carry a sequence number and a deterministic virtual timestamp from a per-mock counter. No wall clock. |
| 10 | `MotorCommand` is a single velocity target (rad/s), no mode enum. The mock enforces configured limits and rejects NaN/Inf and out-of-range values. I3-004 and I3-005 require human review (AGENTS.md section 32, physical actuator control). |
| 11 | I3-001 delivers public headers, documented contracts and a reusable conformance suite exercised against a minimal test double. No mocks until I3-005. |
| 12 | Settings use Core `Configuration` keys `<device>.<endpoint>.<setting>` and `<device>.enabled`; `DeviceManager::config_keys()` feeds the I2 loader allow-list. Minimal settings: sensor sequence start/step and `fail_after_ops`; motor min/max limits and `fail_after_writes`. |
| 13 | `MANIFEST.json` was a packaging artifact referenced by nothing; removed. |
| 14 | Strictly sequential I3-001 to I3-007; one atomic commit per task (`KOS-I3: implement <description>`); evidence and hash recorded in the planning files; the full I2 regression plus I3 tests plus the CI job are the gate each time. |

## KOS-I4 implementation alignment (Claude and ChatGPT, 2026-10-04)

Decided before any KOS-I4 planning file or code exists. These are the architectural source of truth for I4 and govern wherever later text differs. Open design questions are listed after the table.

| # | Decision |
|---|---|
| 1 | KOS-I4 is a transport/protocol integration milestone, software-only, Linux-host, simulation-first. Nexus and Edge are two logical nodes, in one process initially (the architecture must not depend on that). Out: EtherCAT, CAN, STM32/MCU firmware, PREEMPT_RT, ROS2/DDS, real sockets, real hardware, vendor networking stacks, physical actuator deployment. I3 defines what an Endpoint is; I4 defines how an Endpoint can exist remotely. |
| 2 | One abstract `Transport` (send, receive, connection state, deterministic progress) with ONE implementation in I4, `SimulatedTransport`: injectable latency, drop, reorder, duplicate delivery and disconnect, driven by an explicit `step()`; no threads, no sleeping, no wall clock, no real sockets. The protocol layer must not know the transport is simulated. No TCP/UDP in I4. |
| 3 | Core R1.0 is consumed unchanged; no Core modification during I4. Core types are reused where they actually fit; the wire protocol is not forced into Core messaging. If Core cannot express something I4 needs: stop, document the gap, propose a Core change separately. |
| 4 | Hand-written, versioned, little-endian binary protocol: header with magic, protocol version, message type, flags, header length, payload length, sequence and correlation id; hard limits (max frame, max payload, max discovery items, max string length); every incoming frame validated before interpretation; no third-party serialization; never serialize raw structs. Mandatory messages: HELLO/version negotiation, discovery, configure, lifecycle, read, write, status/health/statistics, fault/event notification, heartbeat. Heartbeat is a transport/session mechanism, not an Endpoint operation. |
| 5 | `NodeId` (strong type); an endpoint address is (`NodeId`, `DeviceId`, `EndpointId`); names stay human-readable metadata. The `DeviceRegistry` stays per node. The Nexus side has `RemoteDevice` and `RemoteEndpoint` that implement the unchanged I3 `Device` and `Endpoint` contracts; application code needs no remote-specific type. |
| 6 | A lost link or heartbeat timeout turns the affected Nexus-side remote endpoints into FAULT with a deterministic reason, exactly one ERROR event per fault through the existing DeviceManager path, no silent reconnect or recovery; the way back is shutdown, initialize, start. The remote fault is a derived condition: the Edge endpoint does not become FAULT merely because the Nexus lost the link. Every request has a sequence, a correlation id and a deadline measured in simulated monotonic time. Stale, duplicate, reordered, wrong-node, wrong-endpoint, wrong-type, late (after timeout) and malformed responses are rejected deterministically. |
| 7 | The Edge is the safety authority for actuator commands: it validates limits, state and sequence itself, never trusting the Nexus; validate-before-apply; duplicates are rejected; a command is applied at most once, by an Edge-side duplicate/replay rule (`last_applied_sequence` or a bounded command ledger; a sequence number alone is not enough), tested with duplicate, delayed, reordered and retransmitted commands and a late response after a timeout; on heartbeat loss the Edge drives the actuator command path to a safe state (velocity zero), demonstrated with `MockMotorDevice`. Safety-relevant I4 tasks (I4-006 and every actuator write path) require human safety review, flagged in advance. The I3 safety review stays OPEN independently and I4 must not close it. |
| 8 | No `RemoteDeviceManager`: the existing I3 `DeviceManager` registers `RemoteDevice`s (they are Devices). `RuntimeHost`, `RuntimeManager` and Core are unchanged; no second lifecycle system; a `RemoteEndpoint` remains an I3 Endpoint. |
| 9 | `hardware/transport/` (`transport.hpp`, `frame.hpp`, `codec.hpp`, `protocol.hpp`, `simulated_transport.hpp`) and `hardware/remote/` (`node.hpp`, `remote_device.hpp`, `remote_endpoint.hpp`, `edge_host.hpp`, `remote_registry.hpp`), include roots `kritva/hardware/transport/` and `kritva/hardware/remote/`; no separate `hardware/protocol/` yet; `tests/hardware/`; `examples/nexus_edge_demo`; mocks reused from `hardware/mock`. |
| 10 | Eight strictly sequential tasks, one atomic commit each, no parallel exception: I4-001 architecture, node identity and protocol contract; I4-002 frame format and codec; I4-003 deterministic simulated transport; I4-004 EdgeHost and remote service; I4-005 RemoteDevice and RemoteEndpoint; I4-006 distributed failure and actuator safety (human safety review flag); I4-007 runtime integration and diagnostics; I4-008 Nexus-Edge reference demo. |
| 11 | Process carry-over from I3: this DECISIONS section before I4-001; evidence-based acceptance files per task; every task gate is Build, Unit, Integration, Sanity, KOS-I2 regression, KOS-I3 regression, ASan, UBSan, Mutation, Review, atomic commit; no `Co-Authored-By` trailers; one remediation commit if an audit finds issues. |
| 12 | An explicit session/connection state belongs to the I4 transport/session layer, not to Core: DISCONNECTED, CONNECTING, NEGOTIATING, CONNECTED, DEGRADED. It is not added to Core `Lifecycle`. |
| 13 | Requirement families for the I4 requirements document: NDR node/identity, PR protocol, FR frame/codec, TR transport, ER edge service, RR remote endpoint, FRL failure/reliability, SR safety, RI runtime integration, DR diagnostics, VR verification. |
| 14 | Definition of done: one vertical slice Application, RuntimeHost, DeviceManager, RemoteDevice, RemoteEndpoint, protocol, simulated transport, protocol, EdgeHost, I3 Device and Endpoint, mock hardware, proving HELLO and version negotiation, discovery, configure, initialize, start, sensor read, actuator write, status/health/statistics, heartbeat, an injected link failure, remote endpoint FAULT, Edge actuator at safe zero, late and duplicate messages rejected, no automatic recovery, and explicit shutdown, initialize, start. |

### Open design questions for I4-001 (to be settled with ChatGPT and recorded here before I4-001)

1. The I3 `Endpoint::read`/`write` are synchronous. A `RemoteEndpoint` request therefore completes (or times out) inside the call: proposal is that the caller-visible call issues the request and drives the simulation (`step()`) until the response arrives or the simulated deadline passes, with no asynchronous I/O in I4.
2. How the Edge reaches the actuator safe state on link loss without a Core FAULT: proposal is that the `EdgeHost` stops the affected actuator endpoints (the existing contract: stop zeroes the mock motor), so recovery needs an explicit re-initialize; the alternative is an additive safe-state hook on actuator endpoints.
3. Who owns simulated time and its units (proposal: the `SimulatedTransport` owns a virtual monotonic clock in nanoseconds advanced only by `step()` or an explicit advance), and the heartbeat period and timeout values.
4. Protocol constants: magic, version number scheme, byte order of every field, string encoding, limits.
5. Discovery semantics: a snapshot at HELLO or an explicit request, behaviour when the Edge topology changes, and handling of ids that collide across nodes.
6. Whether the Nexus-side proxy re-reads Edge state on reconnect or only after the explicit re-initialize.
