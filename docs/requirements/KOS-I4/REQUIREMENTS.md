# KOS-I4 Requirements — Traceable Baseline

| ID | Requirement | Task |
|---|---|---|
| NDR-001 | NodeId is a strong validated identity; endpoint address is (NodeId,DeviceId,EndpointId). | I4-001 |
| NDR-002 | Names use [a-z0-9_], 1..64 bytes; DeviceId collisions across nodes are legal. | I4-001 |
| NDR-003 | Every fresh initialization establishes a new session id. | I4-001 |
| PR-001 | HELLO negotiates protocol major/minor and establishes a session. | I4-001 |
| PR-002 | Protocol implements HELLO, discovery, configure, lifecycle, read, write, diagnostics, fault/event and heartbeat. | I4-001 |
| PR-003 | Requests/responses carry sequence and correlation data for deterministic matching. | I4-001 |
| FR-001 | Frame contains K4OS magic, version, type, flags, lengths, sequence and correlation fields. | I4-002 |
| FR-002 | All multibyte fields are little-endian; frame <=64 KiB. | I4-002 |
| FR-003 | Malformed/truncated/oversized/unsupported frames are rejected before use. | I4-002 |
| TR-001 | SimulatedTransport advances only by explicit step/advance and uses monotonic virtual ns. | I4-003 |
| TR-002 | Transport injects latency, drop, reorder and disconnect deterministically. | I4-003 |
| TR-003 | Heartbeat 100 ms, timeout 300 ms and request timeout 100 ms are configuration keys. | I4-003 |
| ER-001 | EdgeHost serves sealed I3 Devices/Endpoints after HELLO. | I4-004 |
| ER-002 | Discovery is a full snapshot and explicit re-discovery is supported. | I4-004 |
| ER-003 | Edge validates address, lifecycle, config and data requests before dispatch. | I4-004 |
| RR-001 | RemoteDevice/RemoteEndpoint implement unchanged I3 contracts. | I4-005 |
| RR-002 | Remote calls synchronously pump until response or simulated deadline. | I4-005 |
| RR-003 | Protocol details do not leak into application-facing Endpoint APIs. | I4-005 |
| FRL-001 | Link/heartbeat loss faults affected Nexus proxies with exactly one ERROR event per fault. | I4-006 |
| FRL-002 | Late, duplicate, stale, unknown-correlation and wrong-address messages are rejected. | I4-006 |
| FRL-003 | No automatic recovery; explicit shutdown/init/start creates a new session. | I4-006 |
| SR-001 | Edge validates actuator limits before every apply. | I4-006 |
| SR-002 | Edge uses a session-scoped last-applied sequence/bounded ledger to prevent replay/duplicate apply. | I4-006 |
| SR-003 | Heartbeat/session timeout stops affected actuator endpoints through existing stop(). | I4-006 |
| RI-001 | Remote devices register through existing DeviceManager; no RemoteDeviceManager. | I4-007 |
| RI-002 | RuntimeHost, RuntimeManager and Core remain unchanged. | I4-007 |
| RI-003 | Remote lifecycle/observation use I3 configuration/status/health/statistics paths. | I4-007 |
| DR-001 | Diagnostics expose NodeId/device/endpoint identity, session, status, health, statistics and last error. | I4-007 |
| DR-002 | Diagnostics do not leak configuration values or unsafe raw frame text. | I4-007 |
| DR-003 | Derived Nexus fault and Edge endpoint state are distinguishable. | I4-007 |
| VR-001 | All I4 tasks pass unit/integration/sanity plus I2/I3 regression. | I4-008 |
| VR-002 | ASan/UBSan and malformed/fault-injection tests pass. | I4-008 |
| VR-003 | Demo proves end-to-end operation, safe stop, rejection and explicit recovery. | I4-008 |

## Family documents

NDR, PR, FR, TR, ER, RR, FRL, SR, RI, DR and VR are the requirement families. IDs are unique across the I4 baseline and each maps to exactly one task.
