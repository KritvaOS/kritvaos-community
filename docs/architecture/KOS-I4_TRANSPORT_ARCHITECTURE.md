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

## Transport and simulated link (I4-003)

`hardware/transport/` adds `transport.hpp` (the abstract `Transport`), `simulated_transport.hpp` (`SimulatedTransport`) and `link_config.hpp` (the `link.*` keys). Like the codec it depends only on Core.

`Transport` is one end of a link that carries **opaque frames** of 1..65536 bytes: `connect`, `disconnect`, `link_state`, `send`, `receive`, `now_ns`, `advance`. It never decodes a header or payload and takes no protocol decision: no session, sequence, correlation, address, topology, endpoint-state, safety or retry logic. A protocol layer encodes with `encode_frame`, hands the bytes to the transport, and decodes what `receive` returns with `decode_frame`; nothing in the transport depends on the codec (a test scans the transport sources for any knowledge of the protocol).

`SimulatedTransport` owns both ends (`nexus()` and `edge()`) and one virtual monotonic nanosecond clock that moves only through `advance(dt)` or `step()`. There are no threads, no sleeping, no wall clock, no `<random>` and no sockets (also scanned by a test). Delivery is pull based, so nothing happens in the background:

- a frame accepted at time `t` is due at `t + latency + extra delay`; `receive()` returns frames whose due time is `<= now_ns()` in (due time, send order) order, so a frame is never visible before its due time and is visible exactly at it;
- `step()` jumps the clock to the earliest due time of any frame in flight (and reports whether a frame is receivable); with nothing in flight it does nothing;
- per direction: fixed latency, a bounded in-flight queue (default 1024), explicit per-frame faults selected by the 0-based index of the accepted send (drop, extra delay, duplicate), and seeded probabilistic drop, duplicate and reorder (permille, xorshift64*, three draws per accepted send in a fixed order); one configuration always gives one behavior;
- reordering is an extra delay on an earlier frame; a duplicate is a second byte-identical copy that follows its original in send order and is not made if the queue has no room;
- the link starts DISCONNECTED; `connect()` brings it up; `disconnect()` (from either end) discards everything in flight in both directions, counted, and `send` then fails with `RESOURCE_UNAVAILABLE`. Disconnect is a connectivity fact only: it does not touch time and implements no safety behavior. Turning link loss or a heartbeat timeout into endpoint behavior is the session and safety layers' job (I4-005 and I4-006);
- `send` failures: `INVALID_ARGUMENT` (empty or over 65536 bytes), `RESOURCE_UNAVAILABLE` (link down, queue full). A frame lost by fault injection is a successful send, as on a real link. Refused sends consume neither a fault index nor a random draw.

`link_timing_from(Configuration)` reads the four `link.*` keys over the defaults (100, 300, 100, 1 ms), accepts integers only, and returns a `LinkTiming` only if every value and the cross-constraints of the specification hold (a caller commits all four or none); `link_config_keys()` is the loader's allow-list. The values are used by the session layer; the transport itself does not use them.

Not in this task: HELLO, sessions, heartbeats, request deadlines, retries (I4-004/I4-005), and the Edge's actuator safety behavior (I4-006).
