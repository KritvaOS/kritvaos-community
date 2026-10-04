# KOS-I4 Transport Architecture

`Transport` provides send/receive, connection/session state and explicit time stepping. I4 has exactly one implementation: `SimulatedTransport`.

`Frame` is a bounded byte sequence. `Codec` encodes/decodes and strictly validates it. `Protocol` maps typed messages to device/session operations.

The protocol never depends on in-process delivery. The simulator can delay, drop, reorder or disconnect frames. Delivery happens only when stepped.

Core reuse is limited to verified R1.0 primitives: `kritva::core::messaging::MessageHeader`, `Topic`, and `kritva::core::time::IClock`. Core has no I4 wire codec or transport contract.

The wire contract (frame layout, message set, status mapping, session states, timing keys) is `docs/architecture/KOS-I4_PROTOCOL.md`; `hardware/transport/` holds its contract headers.
