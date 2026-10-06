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

Out of I4-004, by architect ruling: heartbeat transmission, heartbeat-timeout supervision and the stop-all-actuators action on timeout, FAULT_EVENT, DEGRADED handling and the end-to-end safety campaign are I4-006. A link disconnect is a connectivity fact only and stops nothing in I4-004 (a test asserts it).

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

**Link loss (policy is I4-006).** A request that finds the link down returns RESOURCE_UNAVAILABLE and the session is DISCONNECTED; a proxy does not turn FAULT by itself and the Edge is not told. The explicit way back is shutdown, initialize, start: a shutdown with nobody reachable succeeds locally and marks a pending remote cleanup; the next initialize (a fresh HELLO, which also stops the Edge's running actuators) first brings the Edge endpoint to STOPPED with a best-effort STOP and a SHUTDOWN (sensor endpoints keep running on the Edge when a session ends), then initializes it.
