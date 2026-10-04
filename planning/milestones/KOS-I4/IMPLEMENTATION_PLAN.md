# KOS-I4 Implementation Plan

1. I4-001 freezes identity, session, message set, protocol, timing and configuration keys.
2. I4-002 implements bounded little-endian framing and strict codec validation.
3. I4-003 implements deterministic virtual-time transport and drop/delay/reorder/disconnect injection.
4. I4-004 implements EdgeHost over sealed I3 devices/endpoints.
5. I4-005 implements synchronous RemoteDevice/RemoteEndpoint proxies.
6. I4-006 implements timeout/link-loss/replay semantics and actuator safety.
7. I4-007 integrates through existing DeviceManager and diagnostics.
8. I4-008 implements the deterministic end-to-end demo.

Every gate: Debug/Release build, unit, integration/sanity, full I2 regression, full I3 regression, ASan, UBSan, mutation/fault injection, review, one atomic commit. No Core change.
