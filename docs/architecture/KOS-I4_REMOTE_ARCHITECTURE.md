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
