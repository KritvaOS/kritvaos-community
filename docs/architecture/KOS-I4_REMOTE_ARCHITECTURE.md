# KOS-I4 Remote Architecture

Nexus `DeviceManager` owns `RemoteDevice`; RemoteDevice owns RemoteEndpoints. RemoteEndpoint maps synchronous I3 calls to request/response exchanges.

EdgeHost services requests against sealed I3 Devices/Endpoints. No parallel device/endpoint contract is introduced.

HELLO creates a session id; discovery follows. New explicit initialization invalidates old session state.

Actuator write processing is Edge-authoritative: validate session/address/state, validate command/limits, reject replay/duplicate, apply exactly once, respond. Heartbeat timeout invokes existing Endpoint::stop() on affected actuators.
