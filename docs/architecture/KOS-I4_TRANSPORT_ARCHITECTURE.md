# KOS-I4 Transport Architecture

`Transport` provides send/receive, connection/session state and explicit time stepping. I4 has exactly one implementation: `SimulatedTransport`.

`Frame` is a bounded byte sequence. `Codec` encodes/decodes and strictly validates it. `Protocol` maps typed messages to device/session operations.

The protocol never depends on in-process delivery. The simulator can delay, drop, reorder or disconnect frames. Delivery happens only when stepped.

Core reuse is limited to verified R1.0 primitives: `kritva::core::messaging::MessageHeader`, `Topic`, and `kritva::core::time::IClock`. Core has no I4 wire codec or transport contract.

The wire contract (frame layout, message set, status mapping, session states, timing keys) is `docs/architecture/KOS-I4_PROTOCOL.md`; `hardware/transport/` holds its contract headers.

## Frame and codec layer (I4-002)

`hardware/transport/` provides `frame.hpp` (`FrameHeader`, `decode_frame`, `encode_frame`) and `codec.hpp` (`ByteWriter`, `ByteReader`, one typed payload struct per message with `encode`/`decode`). It depends on Core and the I4-001 contract only (not on a transport, remote proxies, the hardware abstraction or an Edge service) and contains no raw struct serialization.

Layering: `decode_frame` validates the header in the order of the specification and returns a bounded view of exactly `payload_length` bytes; the payload decoders read only that view through `ByteReader`, which can never read outside it, validate every field (finite `f64`, strict UTF-8 without control characters, name charset, enumeration ranges, status range, non-zero ids, no duplicates in a discovery), check every untrusted length against its limit and against the remaining bytes before iterating or allocating, and require exact consumption (trailing bytes and truncation are both `BAD_PAYLOAD`). Encoders reject whatever the decoders would reject, so everything encodable decodes back to an equal value, and the encoding is canonical (decode then encode gives the same bytes). The size of a discovery response is computed before it is encoded.

**Codec boundary.** The codec decides syntax: frame header, field layout, value validity, counts and sizes, duplicates inside a discovery, and the status/message layout (an OK status carries no message, a failing status carries one). The session layer (I4-003 onward) decides: session id validity, sequence admission, correlation and expected response type, address ownership, capability-to-proxy mapping (`UNSUPPORTED`), deadlines, and topology equivalence.
