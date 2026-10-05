# KOS-I4 Changelog

## Baseline
- Established I4 planning baseline and eight sequential tasks.
- Recorded decisions A-F and prior I4 decisions.
- Added requirement, architecture, verification, regression and safety gates.

## Implemented
- I4-001: protocol contract (`docs/architecture/KOS-I4_PROTOCOL.md`), contract headers (`hardware/transport` protocol constants, message types, version, status mapping, session states, link timing; `hardware/remote` NodeId, SessionId, EndpointAddress), and unit, spec-consistency and header tests.
- I4-002: frame format and protocol codec (`frame.hpp`, `codec.hpp`): bounded little-endian reader and writer, strict validation, typed payload encode/decode for every message, discovery size computed before encoding; frame, codec, truncation and fixed-seed fuzz tests.
- I4-003: deterministic simulated transport: abstract `Transport` for opaque frames, `SimulatedTransport` with a virtual clock, latency, drop, reorder, duplicate, bounded queue and disconnect (seeded, repeatable), and validated `link.*` timing configuration; unit, integration and source-hygiene tests.
