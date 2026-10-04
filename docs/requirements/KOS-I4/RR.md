# RR Requirements

See `docs/requirements/KOS-I4/REQUIREMENTS.md` for the normative traceability table.

Family: RR

## Clarifications (planning review)

Two-phase lifecycle. Phase 1 `connect()`: establish a session, HELLO/version negotiation, DISCOVERY, an immutable topology snapshot, construct `RemoteDevice`/`RemoteEndpoint` objects, register with the existing `DeviceManager`. Phase 2 `initialize()`: fresh `SessionId`, HELLO, fresh DISCOVERY, topology equivalence check against the registered snapshot (NodeId, DeviceId, device name, EndpointId, endpoint name, direction, capability id; ordering is not identity unless the protocol defines it), then remote lifecycle initialization; a mismatch fails deterministically. The capability id selects the typed proxy (acceleration, angular velocity, position, motor command); an unknown capability id is `UNSUPPORTED` and discovery fails.
