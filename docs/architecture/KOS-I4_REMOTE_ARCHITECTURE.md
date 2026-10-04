# KOS-I4 Remote Architecture

Nexus `DeviceManager` owns `RemoteDevice`; RemoteDevice owns RemoteEndpoints. RemoteEndpoint maps synchronous I3 calls to request/response exchanges.

EdgeHost services requests against sealed I3 Devices/Endpoints. No parallel device/endpoint contract is introduced.

HELLO creates a session id; discovery follows. New explicit initialization invalidates old session state.

Actuator write processing is Edge-authoritative: validate session/address/state, validate command/limits, reject replay/duplicate, apply exactly once, respond. Heartbeat timeout invokes existing Endpoint::stop() on affected actuators.

Node, session and address identity (`NodeId`, `SessionId`, `EndpointAddress`) is in `hardware/remote/include/kritva/hardware/remote/node.hpp`; the discovery, typed-proxy and topology-equivalence rules are in `docs/architecture/KOS-I4_PROTOCOL.md` section 13.
