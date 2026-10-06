# KOS-I4 Remote Architecture

Nexus `DeviceManager` owns `RemoteDevice`; RemoteDevice owns RemoteEndpoints. RemoteEndpoint maps synchronous I3 calls to request/response exchanges.

EdgeHost services requests against sealed I3 Devices/Endpoints. No parallel device/endpoint contract is introduced.

HELLO creates a session id; discovery follows. New explicit initialization invalidates old session state.

Actuator write processing is Edge-authoritative: validate session/address/state, validate command/limits, reject replay/duplicate, apply exactly once, respond. Heartbeat timeout invokes existing Endpoint::stop() on affected actuators.

Node, session and address identity (`NodeId`, `SessionId`, `EndpointAddress`) is in `hardware/remote/include/kritva/hardware/remote/node.hpp`; the discovery, typed-proxy and topology-equivalence rules are in `docs/architecture/KOS-I4_PROTOCOL.md` section 13.

## Edge service (I4-004)

`hardware/remote/` provides `EdgeHost` (`edge_host.hpp`) and the capability dispatch (`capability_dispatch.hpp`). It links the hardware abstraction and the transport layer and nothing else; a test scans its sources for runtime, manager, clock, thread and networking use.

```text
Nexus frames -> Transport -> EdgeHost -> capability dispatch -> I3 Endpoint -> mock / driver
```

`EdgeHost` is an I4 service, not a Core Component. It serves a plain I3 `DeviceRegistry` that the integrator fills and owns; there is no Edge `RuntimeHost`, `RuntimeManager` or `DeviceManager`, and no I3 contract gained a method for it. It is driven by an explicit `poll()` (no threads, no clock of its own: time is the transport's) and answers through the same transport.

What it decides: session validity, the per-session request/notice watermark, address validation, the typed capability mapping, the write ledger and the Edge-side write order of protocol section 12. What it does not: framing (the codec), delivery (the transport), the I3 rules themselves (every lifecycle, read and write is the endpoint's own, and its error and message are returned unchanged, with text cut to the wire rules). Each request is validated in the order frame, session, payload, admission (watermark), address, then dispatch to the endpoint.

Capability dispatch is the only place that knows concrete typed endpoint contracts: `classify` accepts an endpoint only if it has exactly one capability, one of the four ids of protocol section 13, the matching direction, and really is the typed I3 endpoint (a lookalike with a typed id but another type is UNSUPPORTED); `read_sensor` and `write_motor_command` call the endpoint's own `SensorEndpoint<T>::read` and `ActuatorEndpoint<MotorCommand>::write`. An actuator write therefore always passes the I3 state check and the endpoint's limit validation before anything is applied. A source test keeps `edge_host` free of concrete endpoint types.

Sessions: the first accepted HELLO seals the registry and the Devices. Only an accepted HELLO changes session state; a rejected one has no observable effect on the current session. An accepted HELLO replaces the session as an Edge-side safety action (the previous id invalid; RUNNING actuator endpoints stopped through `Endpoint::stop()`; sequence tracking and ledgers reset; a fresh session sealed in). HELLO is not idempotent (protocol section 9).

Out of I4-004, by architect ruling, and delivered by I4-006 (see the failure and actuator-safety policy below): heartbeat transmission, heartbeat-timeout supervision, the stop of the running actuators on timeout, FAULT_EVENT, DEGRADED and the end-to-end safety campaign. In I4-004 itself a link disconnect stopped nothing; since I4-006 the Edge stops its running actuators at the heartbeat timeout.

## Nexus side (I4-005)

```text
RuntimeHost -> DeviceManager -> RemoteDevice -> RemoteEndpoint -> RemoteSession -> Transport
```

There is no `RemoteDeviceManager`: a `RemoteDevice` is an ordinary I3 `Device` and registers in the existing `DeviceManager`. Everything below is in `hardware/remote/` and depends on the hardware abstraction and the transport layer only (a source test forbids any dependency on `EdgeHost`, the runtime, the manager or a mock).

**RemoteSession** (`remote_session.hpp`) is the Nexus end of one Edge session. `open()` brings the link up, sends HELLO, reads HELLO_ACK (it must match the proposal, the expected Edge node id and the header session id), requests discovery, checks the typed mapping and stores the topology: DISCONNECTED, CONNECTING, NEGOTIATING, CONNECTED, and DISCONNECTED on any failure. `reopen()` does the same with a fresh session and compares the new topology with the registered one as a set. Every request is synchronous and deterministic: it sends one frame, then pumps the transport's virtual clock in `link.pump_quantum_ms` steps (calling the configured peer tick first, so a peer in the same process can handle what is due, without the session knowing what it is) until the matching response or the deadline (send time plus `link.request_timeout_ms`), never past it. A response due exactly at the deadline is accepted; one due later is late.

Response admission follows protocol section 10 and is the Nexus's, not the Edge's: an outstanding-request table keyed by correlation id (the request's sequence) with the expected response type and the deadline. A response is admitted by session, correlation id, expected type and outstanding state, never by comparing its sequence with other frames, so a response that follows a later heartbeat is accepted. Responses that match nothing are dropped and counted: late (the request had timed out), unknown or duplicate (no such outstanding request, or already answered), wrong session, wrong type. A matching response whose payload is malformed, and a PROTOCOL_ERROR that matches the request, fail the request at once with INVALID_ARGUMENT or the Edge's status instead of timing out. Notices (HEARTBEAT, FAULT_EVENT, PROTOCOL_ERROR with correlation 0) use a per-session watermark of the Edge-to-Nexus direction; their content is I4-006's.

A deadline returns the local `TIMEOUT`, never put on the wire. The Nexus never retransmits by itself. A timed-out write has an unknown outcome; the application may call `RemoteMotorCommandEndpoint::retransmit_last_write()`, which resends the SAME request (same sequence, same payload) once, so that the Edge's ledger answers from its cache without applying the command a second time (and, if the Edge never got it, applies it now unless later traffic made it stale). It is refused for a request of a previous session.

**RemoteNode** (`remote_node.hpp`): `connect()` runs `open()` and builds one `RemoteDevice` per discovered device with one typed endpoint per discovered endpoint, selected by the capability id (0x1001 acceleration, 0x1002 angular velocity, 0x1003 position, 0x2001 motor command). The integrator registers the devices with the DeviceManager. Protocol 1.0 does not carry the Edge's setting names, so the integrator declares them per endpoint (`RemoteSettings`); they become the proxies' `setting_names()`, so `DeviceManager::config_keys()` and its scoped `configure` work unchanged. A live period ties initialization to a fresh session: the first endpoint to initialize after none was live calls `reopen()` (NDR-003: a fresh HELLO, a fresh discovery, set equivalence with the registered topology, CONFIGURATION_ERROR and a DISCONNECTED session on any difference); the others share that session; the period ends when every endpoint that began it has shut down.

**RemoteEndpoint** classes derive from the unchanged I3 typed endpoints (`SensorEndpoint<AccelerationSample>` and the others, `ActuatorEndpoint<MotorCommand>`); nothing of the protocol appears in their application-facing API. The I3 state machine, the state checks (a read before RUNNING is refused locally with NOT_READY), the counters and `last_error` are the endpoint's own; the lifecycle hooks and `do_read`/`do_write` are protocol requests, and the Edge's own code and message come back unchanged (a hardware TIMEOUT is mapped by the Edge, see its section). The Nexus keeps no safety authority: a write validates only that the value can be encoded; session, sequence, state, limits and the application remain the Edge's. Floating-point settings cannot be carried by protocol 1.0 and are refused locally.

**Link loss.** The normative policy is I4-006's (the failure and actuator-safety policy below): when a live session ends the node faults every live proxy with `link lost`, once, and records it; a request that finds the link down returns RESOURCE_UNAVAILABLE and ends the session. The explicit way back is shutdown, initialize, start: a shutdown with nobody reachable succeeds locally and marks a pending remote cleanup; the next initialize (a fresh HELLO, which also stops the Edge's running actuators) first brings the Edge endpoint to STOPPED with a best-effort STOP and a SHUTDOWN (sensor endpoints keep running on the Edge when a session ends), then initializes it.

## Failure and actuator-safety policy (I4-006)

Authoritative statement: **the Edge is the final actuator-safety authority. Nexus supervision protects the correctness of the remote representation; Edge supervision protects the physical actuator.** The normative text, including the table of which frames refresh liveness, is in `docs/architecture/KOS-I4_PROTOCOL.md` section 11; this section places it.

```text
                 Nexus                                 Edge
        supervision (service())                supervision (poll())
                 |                                       |
   DEGRADED (observation) / link lost          DEGRADED (observation) / timeout
                 |                                       |
   live remote endpoints -> FAULT         running actuators -> Endpoint::stop()
   + one link record                      sensors untouched, no endpoint faulted
```

- **Edge** (`EdgeHost::poll()`): heartbeat sending, the `>=` heartbeat timeout as one transition (session permanently invalid, ledgers and watermark cleared, every RUNNING actuator stopped through the existing `Endpoint::stop()`), `link_state()` (CONNECTED, DEGRADED, DISCONNECTED; observation only), the liveness rules, and FAULT_EVENT through an I3 fault listener on each served endpoint (queued after the response, flushed at the next drive; a shared token makes it safe whichever of the host and the devices is destroyed first). Nothing stops an actuator because of a duplicate or replayed frame, a heartbeat, a FAULT_EVENT or a Nexus-side close.
- **Nexus** (`RemoteSession::service()`, `RemoteNode::service()`; also called at every step of the synchronous request pump): reads what is due, supervises (link down, `>=` timeout, DEGRADED), sends at most one heartbeat per call, and on the end of a live session tells the node, which faults the live remote endpoints with a deterministic reason, once, and records one `LinkLossRecord`. FAULT_EVENTs fault the matching live endpoint with `remote fault: <reason>`.
- **Sensors** keep their state when the Edge's session ends (ruled by the architect: nothing about a lost link makes a sensor unsafe). The Nexus-side recovery cleanup of I4-005 brings a surviving Edge sensor back into a clean I3 lifecycle: shutdown, initialize, start with a fresh HELLO.
- **No automatic recovery** anywhere, and a fresh HELLO never clears a FAULT.
- Limits, stated: supervision runs only when driven (an integrator that spends virtual time without a request calls `service()` and `poll()`); a backlog of frames is handled with the time of the drive that handles it; the Edge learns that the Nexus closed a session only by its heartbeat timeout.

## Runtime integration and diagnostics (I4-007)

The production path, which the I4-008 demo must use and not bypass:

```text
Application
   -> RuntimeHost -> DeviceManager -> RemoteDevice -> RemoteEndpoint -> RemoteNode (RemoteSession)
   -> LoopbackLink -> SimulatedTransport -> EdgeHost -> I3 Device / Endpoint -> mock hardware
```

Nothing in `runtime/`, `core/`, `hardware/abstraction`, `hardware/mock` or `hardware/transport` changed. There is no RemoteDeviceManager, no Edge runtime, no second lifecycle manager, component registry or safety manager: a `RemoteDevice` is registered in the existing `DeviceManager`, which the existing `RuntimeHost` hosts, and configure, initialize and start keep the I2 and I3 semantics and the I3 deterministic order.

**Configuration.** The link timing keys (`link.*`) are read first, because the timing is proposed in HELLO before anything is discovered (`link_timing_from`: an invalid timing is a CONFIGURATION_ERROR before a single frame is sent). After discovery the whole configuration is validated by the I2 loader against the allow-list `DeviceManager::config_keys()` + `link_config_keys()` + `runtime.*`, so typos are rejected, and given to the host.

**Driving is explicit** (a gap in driving is a gap in supervision). `RemoteNode::service()` is the single production supervision entry; `EdgeHost::poll()` is the Edge's. `LoopbackLink` (`loopback_link.hpp`) is the one same-process composition utility that calls them: `advance(dt)` repeats, until `dt` of virtual time is consumed, (1) the transport clock moves by `min(quantum, remaining)`, (2) `EdgeHost::poll()`, (3) `RemoteNode::service()`. The order is fixed, documented and tested (an answer the Edge sends in a step is read by the Nexus in the same step; a heartbeat the Nexus sends is read by the Edge in the next). It owns nothing, is not a Component, Device or Endpoint, has no clock, thread or sleep, and decides nothing about lifecycle, safety, reconnection or recovery; a source scan enforces this. The Nexus's synchronous request pump uses `peer_tick()` of the same object. Nobody calls supervision from a constructor, a destructor, a callback or a Core lifecycle call.

**Diagnostics are passive observation, never a second safety mechanism.**
- `diagnose(const RemoteNode&)` (`remote_diagnostics.hpp`) returns a coherent copy: the Edge node, the session, the link state, the timing, the virtual time, the link and node statistics, the link-loss records (a copy: no public API can change or clear the node's own) and, per remote endpoint, its address, the I3 snapshot of the proxy (state, status, health and its detail, statistics, last error, capabilities) and the `FaultOrigin`.
- `diagnose(const EdgeHost&)` (`edge_diagnostics.hpp`) returns the Edge's own view: node, session, link state, timing, statistics and the I3 snapshots of the served devices, which is the authority for the actuators. It is the diagnostic side of the same-process simulation: `RemoteNode` has no reference to an `EdgeHost`, and the two diagnostics live in separate headers that do not know each other.
- Both are PASSIVE: they take const references, send and read no frame, call neither `service()` nor `poll()` nor any request or lifecycle operation, and change no state, counter, record, watermark, deadline, session or transport state; they cannot refresh liveness, reconnect, restart, clear a FAULT, reset statistics, clear a ledger or acknowledge a condition. Tests assert this in every state, and a source scan forbids every active call in their sources.
- `describe()` renders a snapshot as deterministic text with the I3 sanitizer (control characters, DEL and double quotes replaced; device and endpoint names too, because a snapshot is plain data). No configuration value and no raw frame content is ever included.
- Active observation of the Edge over the link (an OBSERVE_REQUEST from diagnostics) is deliberately NOT part of I4-007: it would give an apparently observational API a liveness side effect on the Edge, and needs its own requirement.

**Fault origin.** `FaultOrigin` {NONE, LINK_LOST, SESSION_CLOSED, REMOTE_FAULT, LOCAL_FAILURE} is metadata of the Nexus-side proxy, recorded explicitly when the Nexus puts an endpoint into FAULT (never parsed from the reason text) and not part of the I3 Endpoint contract. It is fixed for the fault episode: it does not change while the endpoint stays FAULT (a later link loss, close or FAULT_EVENT cannot overwrite it), and shutdown or initialize starts a new episode. LOCAL_FAILURE means the proxy faulted by its own I3 rules (for example a lifecycle request the Edge refused, or an operation that failed because the link dropped during it). The three reasons stay distinguishable everywhere (health detail, link record, text): `link lost`, `session closed`, `remote fault: <reason>`.

**Edge authority versus the Nexus representation.** It is legitimate that for a while the Edge's actuator is STOPPED (its own timeout) while the Nexus proxy is still RUNNING, until the Nexus supervision is driven; the two diagnostics show exactly that, and nothing synchronizes them behind the application's back. `RemoteSession::close()` is a Nexus-local event (the proxies go FAULT with `session closed`); protocol 1.0 has no termination message, so the Edge keeps its session and its running actuator until its own heartbeat timeout stops it. No diagnostic or runtime observation ever causes a reopen, initialize or start: recovery is only shutdown, initialize, start with a fresh session.
